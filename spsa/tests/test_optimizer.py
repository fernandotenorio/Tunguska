import copy
import random
import unittest

from spsa.optimizer import prepare, restore_random, stochastic, update


def config():
    return dict(algorithm=dict(iterations=1000, A_ratio=0.1, alpha=0.602, gamma=0.101),
                match=dict(pairs_per_iteration=2), selected=["x", "y"],
                parameters={n: dict(min=-100, max=100, c_end=1, r_end=0.1) for n in ("x", "y")})


class OptimizerTests(unittest.TestCase):
    def test_hand_computed_update_draw_and_sign(self):
        cfg = config()
        theta = dict(x=10., y=20.)
        step = dict(details=dict(x=dict(a=2., c=4., delta=1), y=dict(a=3., c=2., delta=-1)))
        self.assertEqual(update(cfg, theta, step, [2, 0]), dict(x=10.5, y=18.5))
        self.assertEqual(update(cfg, theta, step, [-2, 0]), dict(x=9.5, y=21.5))
        self.assertEqual(update(cfg, theta, step, [0, 0]), theta)
        cfg["match"]["pairs_per_iteration"] = 4
        self.assertEqual(update(cfg, theta, step, [2, 0, 2, 0]), dict(x=10.5, y=18.5))

    def test_incomplete_iteration_rejected(self):
        with self.assertRaises(ValueError):
            update(config(), dict(x=0, y=0), dict(details={}), [2])

    def test_gain_endpoints(self):
        cfg = config()
        step = prepare(cfg, dict(x=0, y=0), 1000, random.Random(1))
        for d in step["details"].values():
            self.assertEqual(d["c"], 1)
            self.assertEqual(d["a"], 0.1)

    def test_rounding_unbiased_and_seed_resume(self):
        rng = random.Random(3)
        mean = sum(stochastic(-1.7, rng) for _ in range(50000)) / 50000
        self.assertAlmostEqual(mean, -1.7, delta=0.01)
        saved = rng.getstate()
        a = prepare(config(), dict(x=0.5, y=10.25), 12, rng)
        self.assertEqual(a, prepare(config(), dict(x=0.5, y=10.25), 12, restore_random(saved)))

    def test_bounds_and_unselected(self):
        cfg = config()
        cfg["selected"] = ["x"]
        step = prepare(cfg, dict(x=100, y=20), 1, random.Random(4))
        self.assertTrue(step["details"]["x"]["clipped"])
        for side in ("plus", "minus"):
            self.assertLessEqual(step[side]["x"], 100)
            self.assertEqual(step[side]["y"], 20)
        after = update(cfg, dict(x=100, y=20), step, [2, 2])
        self.assertEqual(after["y"], 20)
        self.assertLessEqual(after["x"], 100)

    def test_noisy_quadratic_improves(self):
        cfg = config()
        cfg["algorithm"]["iterations"] = 4000
        cfg["match"]["pairs_per_iteration"] = 32
        cfg["parameters"]["x"]["r_end"] = cfg["parameters"]["y"]["r_end"] = 0.2
        theta = dict(x=-10., y=10.)
        rng = random.Random(73)
        def quality(point):
            return -((point["x"] - 3)**2 + (point["y"] + 2)**2)
        initial = quality(theta)
        for k in range(1, 4001):
            step = prepare(cfg, theta, k, rng)
            probability = max(0.05, min(0.95, 0.5 + (quality(step["plus"]) - quality(step["minus"])) / 200))
            pairs = [sum(1 if rng.random() < probability else -1 for _ in range(2)) for _ in range(32)]
            theta = update(cfg, theta, step, pairs)
        self.assertGreater(quality(theta), initial * 0.1)
