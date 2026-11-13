"""Exercise the checked-in BASIC source with a small test-only interpreter.

This covers only the BASIC statements used by TRUN, not calculator timing,
rendering, tokenization, or the behavior of a real fx-CG50.
"""
import re
import unittest
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "realtime" / "TRUN.cbas"


class EndSimulation(Exception):
    pass


def expression(text):
    text = text.replace("RanInt#", "randint").replace("Getkey", "getkey()")
    text = text.replace("≠", "!=").replace("≥", ">=").replace("≤", "<=")
    text = re.sub(r"(\d)([A-Z])", r"\1*\2", text)
    return re.sub(r"(?<![<>!=])=(?!=)", "==", text)


def compile_basic(source):
    lines = []
    indent = 0
    pending_if = None
    for raw in source.splitlines():
        line = raw.strip()
        if not line or line.startswith("'"):
            continue
        if line.startswith("If "):
            pending_if = expression(line[3:])
            continue
        if line == "Then":
            if pending_if is None:
                raise ValueError("Then without If")
            lines.append("    " * indent + "if " + pending_if + ":")
            indent += 1
            pending_if = None
        elif line.startswith("While "):
            lines.append("    " * indent + "while " + expression(line[6:]) + ":")
            indent += 1
        elif line in ("IfEnd", "WhileEnd"):
            indent -= 1
            if indent < 0:
                raise ValueError("Unbalanced block")
        elif line == "ClrText":
            lines.append("    " * indent + "clear()")
        elif line.startswith("Locate "):
            x, y, value = line[7:].split(",", 2)
            lines.append("    " * indent + "locate(" + expression(x) + "," +
                         expression(y) + "," + value + ")")
        elif "→" in line:
            value, variable = line.split("→")
            lines.append("    " * indent + variable + " = " + expression(value))
        else:
            raise ValueError("Unsupported statement: " + line)
    if indent or pending_if is not None:
        raise ValueError("Unclosed block")
    return compile("\n".join(lines), str(SOURCE), "exec")


def simulate(hazard, lane=2, keys=None, limit=161):
    keys = keys or {}
    values = iter([lane, hazard] * 4)
    displays = []
    poll = [0]

    def getkey():
        poll[0] += 1
        if poll[0] > limit:
            raise EndSimulation()
        return keys.get(poll[0], 0)

    def randint(low, high):
        value = next(values)
        assert low <= value <= high
        return value

    state = {"getkey": getkey, "randint": randint,
             "clear": lambda: None,
             "locate": lambda *args: displays.append(args)}
    try:
        exec(compile_basic(SOURCE.read_text()), state)
    except EndSimulation:
        pass
    return state, displays


class RealtimeTests(unittest.TestCase):
    def test_collisions_and_correct_actions(self):
        for hazard, key in ((2, 28), (3, 28), (4, 37)):
            with self.subTest(hazard=hazard):
                game, _ = simulate(hazard)
                self.assertEqual(game["G"], 0)
                game, _ = simulate(hazard, keys={145: key})
                self.assertEqual((game["G"], game["S"]), (1, 10))

    def test_lane_change_and_release(self):
        game, _ = simulate(2, keys={140: 38, 141: 38})
        self.assertEqual((game["A"], game["G"]), (1, 1))
        game, _ = simulate(2, keys={130: 27, 140: 27})
        self.assertEqual((game["A"], game["G"]), (3, 1))

    def test_coin_and_quit(self):
        game, _ = simulate(1)
        self.assertEqual((game["C"], game["S"]), (1, 35))
        game, displays = simulate(1, keys={5: 29})
        self.assertEqual(game["G"], 0)
        self.assertIn((1, 2, "RUN ENDED"), displays)

    def test_draws_first_hazard_at_start(self):
        _, displays = simulate(4, limit=1)
        self.assertIn((14, 2, "B"), displays)


if __name__ == "__main__":
    unittest.main()
