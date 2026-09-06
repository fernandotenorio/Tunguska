from pathlib import Path
import tempfile
import unittest

from spsa.config import catalog, load, write_template


class ConfigTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "config.toml"
        write_template(self.path)
        text = self.path.read_text()
        start, end = text.index("[paths]"), text.index("[match]")
        text = text[:start] + '[paths]\nengine="engine.exe"\ncutechess="cutechess.exe"\nopenings="book.epd"\nruns="runs"\n\n' + text[end:]
        self.path.write_text(text)
        for name in ("engine.exe", "cutechess.exe", "book.epd"):
            (self.root / name).write_text("fixture")

    def test_complete_catalog_and_profiles(self):
        for profile in ("search", "pieces", "time"):
            cfg = load(self.path, profile, 1000)
            self.assertEqual(set(cfg["parameters"]), set(catalog()))
            self.assertTrue(all(cfg["parameters"][n]["group"] == profile for n in cfg["selected"]))

    def test_stale_invalid_and_unknown_settings(self):
        text = self.path.read_text()
        for before, after in [('default = 15', 'default = 16'), ('alpha = 0.602', 'alpha = nan'),
                              ('threads = 1', 'threads = 0'), ('min = 8', 'min = 0'),
                              ('r_end = 0.002', 'r_end = -1'), ('gamma = 0.101', 'gamma = 0.3'),
                              ('concurrency = 1', 'concurency = 1')]:
            self.path.write_text(text.replace(before, after, 1))
            with self.subTest(after=after):
                with self.assertRaises((ValueError, KeyError)):
                    load(self.path, "search", 1000)

    def test_budget_required_only_for_run(self):
        self.assertEqual(load(self.path, "search")["algorithm"]["iterations"], 0)
        with self.assertRaises(ValueError): load(self.path, "search", -1)
