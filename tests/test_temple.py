import unittest
from unittest.mock import patch

import TEMPLE


class GameTests(unittest.TestCase):
    def game(self, tile, lane=1):
        return {"lane": lane, "score": 0, "coins": 0, "distance": 0,
                "alive": True, "tile": tile}

    def test_hazards_require_correct_action(self):
        for kind, safe in (("GAP", "8"), ("LOG", "8"), ("BRANCH", "2")):
            with self.subTest(kind=kind):
                game = self.game((kind, 1))
                TEMPLE.step(game, safe)
                self.assertTrue(game["alive"])
                self.assertEqual(game["score"], 10)
                game = self.game((kind, 1))
                TEMPLE.step(game, "5")
                self.assertFalse(game["alive"])
                self.assertEqual(game["distance"], 0)

    def test_lane_change_avoids_hazard_and_boundaries_clamp(self):
        game = self.game(("GAP", 1))
        TEMPLE.step(game, "4")
        self.assertTrue(game["alive"])
        self.assertEqual(game["lane"], 0)
        game["tile"] = ("CLEAR", 0)
        TEMPLE.step(game, "4")
        self.assertEqual(game["lane"], 0)
        game["lane"] = 2
        TEMPLE.step(game, "6")
        self.assertEqual(game["lane"], 2)

    def test_coins_only_collected_in_same_lane(self):
        game = self.game(("COIN", 1))
        TEMPLE.step(game, "5")
        self.assertEqual((game["coins"], game["score"]), (1, 35))
        game["tile"] = ("COIN", 0)
        TEMPLE.step(game, "5")
        self.assertEqual((game["coins"], game["score"]), (1, 45))

    def test_invalid_and_dead_game_do_not_advance(self):
        game = self.game(("GAP", 1))
        before = game.copy()
        self.assertFalse(TEMPLE.step(game, "9"))
        self.assertEqual(game, before)
        TEMPLE.step(game, "5")
        before = game.copy()
        self.assertFalse(TEMPLE.step(game, "8"))
        self.assertEqual(game, before)

    def test_generator_and_interactive_game(self):
        for kind in range(5):
            with patch.object(TEMPLE.random, "randint", side_effect=[kind, 2]):
                self.assertEqual(TEMPLE.make_tile()[1], 2)
        with patch.object(TEMPLE, "make_tile", return_value=("GAP", 1)):
            with patch("builtins.input", side_effect=["x", "8", "0"]):
                with patch("builtins.print") as output:
                    TEMPLE.main()
        printed = " ".join(str(call) for call in output.call_args_list)
        self.assertIn("USE 0,2,4,5,6,8", printed)
        self.assertIn("RUN ENDED", printed)


if __name__ == "__main__":
    unittest.main()
