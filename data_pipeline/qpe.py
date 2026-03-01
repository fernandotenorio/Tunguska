"""
qpe.py

QPE - Quiet Position Extractor

Analyze EPD and extract those that are quiet.

Requirements:
    Python-chess==0.31.2
"""


import subprocess
import time
from pathlib import Path
import argparse
import logging

import chess
import chess.pgn
import chess.engine


APP_NAME = 'QPE - Quiet Position Extractor'
APP_VERSION = 'v0.18.beta'


def get_time_h_mm_ss_ms(time_delta_ns):
    time_delta_ms = time_delta_ns // 1000000
    s, ms = divmod(time_delta_ms, 1000)
    m, s = divmod(s, 60)
    h, m = divmod(m, 60)

    return '{:01d}h:{:02d}m:{:02d}s:{:03d}ms'.format(h, m, s, ms)


def parse_epd_line(epd_line):
    # 1. Extract the FEN part (The first 4 elements separated by spaces)
    parts = epd_line.split()
    fen_part = " ".join(parts[:4])
    
    # 2. Reconstruct a valid FEN by adding dummy halfmove and fullmove clocks
    dummy_fen = f"{fen_part} 0 1"
    
    # 3. Create the python-chess board
    board = chess.Board(dummy_fen)
    
    # 4. Extract the Game Result (z) from the c1 tag
    if "c1 1-0" in epd_line:
        result = 1.0  # White wins
    elif "c1 0-1" in epd_line:
        result = 0.0  # Black wins
    elif "c1 1/2-1/2" in epd_line:
        result = 0.5  # Draw
    else:
        result = None # Fallback if something went wrong
    return board, {}, result


def save_evaluated_epd(epd, filename):
    with open(filename, 'a') as e:
        e.write(f'{epd}\n')


def get_epd(infn):
    epds = []
    evaluated_file = Path(infn)
    if evaluated_file.is_file():
        with open(infn) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    board, _ = chess.Board().from_epd(line)
                except ValueError:
                    board = chess.Board(line)
                epds.append(board.epd())
    return epds



def stockfish_staticeval(engineprocess, board):
    """
    command: eval
    result: Total evaluation: 0.13 (white side)
    """
    staticscorecp = None

    fen = board.fen()

    engineprocess.stdin.write('ucinewgame\n')
    engineprocess.stdin.write(f'position fen {fen}\n')

    engineprocess.stdin.write('eval\n')
    for lines in iter(engineprocess.stdout.readline, ''):
        line = lines.strip()
        if 'total evaluation' in line.lower():
            scoreline = line.split('Total evaluation:')[1].strip()
            score = float(scoreline.split('(')[0].strip())
            staticscorecp = int(100*score)
            break

    if staticscorecp is None:
        return None

    return staticscorecp if board.turn else -staticscorecp


def tactical_move(board=None, epdinfo=None, move=None):
    """
    Evaluate if the move in epd or a move from pv is a tactical move.
    Tactical moves: capture or check or promote or mate.

    :param board: python-chess board object
    :param epdinfo: the opcode/operand of the EPD, a dictionary or {}
    :param move: the move from the pv in SAN format
    :return: True if move is tactical othewise False
    """
    # Case 1. A move in the pv is evaluated.
    if move is not None:
        if 'x' in move or '+' in move or '=' in move or '#' in move:
            return True

    # Case 2. The bm in the EPD is evaluated.
    else:
        if 'bm' not in epdinfo:
            return False

        sanbm = [board.san(m) for m in epdinfo['bm']]
        for m in sanbm:
            if 'x' in m or '+' in m or '=' in m or '#' in m:
                return True

    return False


def runengine(engine_file, engineoption, epdfile, movetimems,
              outputepd, pvlen, scoremargin, use_static_eval,
              lowpvfn, evaluated_file):

    pos_num = 0

    evaluated_epds = set(get_epd(evaluated_file))

    folder = Path(engine_file).parents[0]
    engine = chess.engine.SimpleEngine.popen_uci(engine_file, cwd=folder)

    if engineoption is not None:
        for opt in engineoption.split(','):
            optname = opt.split('=')[0].strip()
            optvalue = opt.split('=')[1].strip()
            engine.configure({optname: optvalue})

    limit = chess.engine.Limit(time=movetimems / 1000)

    # second process for static eval
    engineprocess = subprocess.Popen(
        engine_file,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1
    )

    # ---- OPEN FILES ONCE ----
    evaluated_fh = open(evaluated_file, "a")
    output_fh = open(outputepd, "a", buffering=1)

    eval_buffer = []
    FLUSH_EVERY = 10

    with open(epdfile) as f:
        for line in f:

            line = line.strip()
            if not line:
                continue

            board, epdinfo, result = parse_epd_line(line)

            epd = board.epd()
            epdline = board.epd()
            fen = board.fen()
            pos_num += 1

            if pos_num % 5000 == 0:
                print(f'pos: {pos_num}')

            if epd in evaluated_epds:
                continue

            accept = True
            ismate = False

            # --- skip conditions before search ---
            if board.is_attacked_by(not board.turn, board.king(board.turn)):
                accept = False
            elif tactical_move(board=board, epdinfo=epdinfo, move=None):
                accept = False
            else:
                pv, score, mate_score = '', None, None
                with engine.analysis(board, limit) as analysis:
                    for info in analysis:
                        if ('upperbound' not in info
                                and 'lowerbound' not in info
                                and 'score' in info
                                and 'pv' in info):

                            pv = info['pv']

                            if info['score'].is_mate():
                                ismate = True
                                mate_score = info['score']
                            else:
                                mate_score = None

                            score = info['score'].relative.score(
                                mate_score=32000
                            )
                            score_w = info['score'].white().score()

                if ismate:
                    accept = False

                # --- static eval filter ---
                elif (use_static_eval and score is not None and
                      'stockfish' in engine.id['name'].lower()):

                    staticeval = stockfish_staticeval(engineprocess, board)
                    if staticeval is None:
                        accept = False
                    else:
                        absdiff = abs(score - staticeval)
                        if absdiff > scoremargin:
                            accept = False

                # --- pv length check ---
                elif len(pv) < pvlen:
                    accept = False

                    if pvlen >= 2 and lowpvfn:
                        ucipv = [str(m) for m in pv]
                        with open(lowpvfn, 'a') as s:
                            s.write(
                                f'{epdline} Acms {movetimems}; '
                                f'C0 "pv too short: {len(ucipv)}";\n'
                            )
                else:
                    # --- check tactical moves inside pv ---
                    temp_board = board.copy()
                    for i, m in enumerate(pv):
                        if i >= pvlen:
                            break

                        sanmove = temp_board.san(m)

                        if tactical_move(move=sanmove):
                            accept = False
                            break

                        temp_board.push(m)

            # ---- SINGLE CHECKPOINT LOCATION ----
            evaluated_epds.add(epd)
            eval_buffer.append(epdline)

            if len(eval_buffer) >= FLUSH_EVERY:
                evaluated_fh.write("\n".join(eval_buffer) + "\n")
                evaluated_fh.flush()
                eval_buffer.clear()

            # ---- SINGLE OUTPUT LOCATION ----
            if accept:               
                output_fh.write("|".join([fen, str(score_w), str(result)]) + "\n")

    # final flush
    if eval_buffer:
        evaluated_fh.write("\n".join(eval_buffer) + "\n")

    evaluated_fh.close()
    output_fh.close()

    engine.quit()
    engineprocess.stdin.write('quit\n')
    engineprocess.stdin.flush()



def main():
    parser = argparse.ArgumentParser(
        prog='%s %s' % (APP_NAME, APP_VERSION),
        description='Analyze epd and save quiet positions.',
        epilog='%(prog)s')
    parser.add_argument('--input', required=True, help='input epd file')
    parser.add_argument('--outputepd', required=False,
                        help='output epd file in append mode, default=out.epd',
                        default='out.epd')
    parser.add_argument('--engine', required=True, help='input engine file')
    parser.add_argument(
        '--engine-option', required=False,
        help='input engine options, e.g '
             '--engine-option "Threads=1, Hash=128, Debug Log File=log.txt"')
    parser.add_argument('--movetimems', required=False, type=int,
                        help='input analysis time in ms, default=1000',
                        default=1000)
    parser.add_argument('--pvlen', required=False, type=int,
                        help='input pv length to check moves, default=4',
                        default=4)
    parser.add_argument('--scorecp-margin', required=False, type=int,
                        help='input score margin in cp (centipawn) for the '
                             'score delta of static eval and search score. '
                             'If delta is above score margin, the position '
                             'will not be saved. Default=100',
                        default=100)
    parser.add_argument('--log', action="store_true",
                        help='a flag to enable logging')
    parser.add_argument('--static-eval', action="store_true",
                        help='a flag to enable the use of static eval '
                             'in extracting quiet positions')
    parser.add_argument('--evaluated-file',
                        required=False,
                        default='evaluated.epd',
                        help='file storing already evaluated positions')
            

    args = parser.parse_args()
    epd_file = args.input
    engine_file = args.engine
    outepd_file = args.outputepd
    movetimems = args.movetimems

    # DO NOT delete existing epdoutput file
    tmpfn = Path(outepd_file)
    # tmpfn.unlink(missing_ok=True)

    # Save epd where engine pv length is below required pv length, append mode.
    lowpvfn = None #'low_pvlength.epd'

    if args.log:
        logging.basicConfig(
            level=logging.DEBUG,
            filename='log_quietopeningextractor.txt',
            filemode='w')

    timestart = time.perf_counter_ns()

    print('Analysis starts...')
    runengine(engine_file, args.engine_option, epd_file, movetimems,
              outepd_file, args.pvlen, args.scorecp_margin, args.static_eval,
              lowpvfn, args.evaluated_file)
    print('Analysis done!')

    elapse = time.perf_counter_ns() - timestart
    print(f'Elapse: {get_time_h_mm_ss_ms(elapse)}')


if __name__ == '__main__':
    main()