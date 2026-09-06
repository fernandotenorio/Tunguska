"""Pure SPSA arithmetic. All values are in integer UCI wire units."""
import math
import random


def clip(value, p):
    return min(p["max"], max(p["min"], value))


def nearest(value):
    return math.floor(value + 0.5)


def stochastic(value, rng):
    base = math.floor(value)
    return base + int(rng.random() < value - base)


def restore_random(state):
    def tuples(value):
        return tuple(tuples(x) for x in value) if isinstance(value, (list, tuple)) else value
    rng = random.Random()
    rng.setstate(tuples(state))
    return rng


def prepare(cfg, theta, k, rng):
    alg = cfg["algorithm"]
    budget = alg["iterations"]
    A = alg["A_ratio"] * budget
    plus = {n: nearest(v) for n, v in theta.items()}
    minus = dict(plus)
    details = {}
    for name in cfg["selected"]:
        p = cfg["parameters"][name]
        c = p["c_end"] * (budget / k)**alg["gamma"]
        a = p["r_end"] * p["c_end"]**2 * ((A + budget) / (A + k))**alg["alpha"]
        delta = rng.choice((-1, 1))
        raw_plus, raw_minus = theta[name] + c * delta, theta[name] - c * delta
        # Shared uniform yields unbiased rounding of each candidate and avoids
        # gratuitous differences when the two real-valued candidates coincide.
        uniform = rng.random()
        def quantize(v):
            v = clip(v, p)
            return math.floor(v) + int(uniform < v - math.floor(v))
        plus[name], minus[name] = quantize(raw_plus), quantize(raw_minus)
        details[name] = dict(theta=theta[name], c=c, a=a, delta=delta,
                             plus=plus[name], minus=minus[name],
                             clipped=raw_plus != clip(raw_plus, p) or raw_minus != clip(raw_minus, p),
                             identical=plus[name] == minus[name])
    if plus == minus:
        raise ValueError("Entire perturbation rounded to identical engines; increase c_end or widen bounds")
    return dict(k=k, plus=plus, minus=minus, details=details)


def update(cfg, theta, step, pair_results):
    if len(pair_results) != cfg["match"]["pairs_per_iteration"]:
        raise ValueError("Cannot update an incomplete iteration")
    if any(x not in (-2, -1, 0, 1, 2) for x in pair_results):
        raise ValueError("Invalid pair score")
    signal = sum(pair_results) / len(pair_results)
    result = dict(theta)
    for name, d in step["details"].items():
        result[name] = clip(theta[name] + d["a"] * signal * d["delta"] / d["c"], cfg["parameters"][name])
        if not math.isfinite(result[name]):
            raise ValueError("Nonfinite optimizer update")
    return result
