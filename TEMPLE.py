"""A small, asset-free, turn-based runner for the Casio fx-CG50."""
import random


def new_game():
    return {"lane": 1, "score": 0, "coins": 0, "distance": 0,
            "alive": True, "tile": make_tile()}


def make_tile():
    # Every tile has at most one hazard, leaving at least two safe lanes.
    kind = random.randint(0, 4)
    lane = random.randint(0, 2)
    if kind == 0:
        return ("CLEAR", lane)
    if kind == 1:
        return ("COIN", lane)
    if kind == 2:
        return ("GAP", lane)
    if kind == 3:
        return ("LOG", lane)
    return ("BRANCH", lane)


def step(game, action):
    """Resolve one visible tile. Return False for invalid input."""
    if action not in ("2", "4", "5", "6", "8") or not game["alive"]:
        return False
    lane = game["lane"]
    if action == "4":
        lane = max(0, lane - 1)
    elif action == "6":
        lane = min(2, lane + 1)
    game["lane"] = lane
    kind, tile_lane = game["tile"]
    if lane == tile_lane:
        if kind == "COIN":
            game["coins"] += 1
            game["score"] += 25
        elif kind in ("GAP", "LOG") and action != "8":
            game["alive"] = False
        elif kind == "BRANCH" and action != "2":
            game["alive"] = False
    if game["alive"]:
        game["distance"] += 1
        game["score"] += 10
        game["tile"] = make_tile()
    return True


def show(game):
    kind, tile_lane = game["tile"]
    lanes = ["-", "-", "-"]
    if kind != "CLEAR":
        lanes[tile_lane] = "!" if kind != "COIN" else "$"
    runner = ["-", "-", "-"]
    runner[game["lane"]] = "R"
    print("SCORE", game["score"], "COINS", game["coins"])
    print("NEXT", " ".join(lanes), kind)
    print(" YOU", " ".join(runner))
    print("4/6 MOVE 8 JUMP")
    print("2 SLIDE 5 RUN 0 END")


def main():
    print("TEMPLE RUNNER")
    print("GAP/LOG: JUMP")
    print("BRANCH: SLIDE")
    print("OR MOVE AWAY")
    game = new_game()
    while game["alive"]:
        show(game)
        action = input("? ").strip()
        if action == "0":
            print("RUN ENDED")
            break
        if not step(game, action):
            print("USE 0,2,4,5,6,8")
    if not game["alive"]:
        print("GAME OVER")
    print("SCORE", game["score"], "COINS", game["coins"])
    print("DISTANCE", game["distance"])
