import zmq
import json
import random
import time

ctx = zmq.Context()
sock = ctx.socket(zmq.REP)
sock.bind("tcp://*:5555")
print("AI server listening on 5555")

while True:
    try:
        msg = sock.recv()
        req = json.loads(msg.decode("utf-8"))

        winner = req.get("winner")
        if winner:
            print(f"[GameOver] winner={winner}")
            sock.send_json({"status": "ok"})
            continue

        print("Received from C++:", req)

        moves = req.get("possible_moves", [])
        if moves:
            idx = random.randrange(len(moves))
            sel = moves[idx]
            resp = {
                "status": "ok",
                "selected_index": idx,
                "destination": sel.get("dest"),
                "transport": sel.get("transport", 0)
            }
        else:
            resp = {"status": "ok", "selected_index": None, "destination": None, "transport": 0}

        # optional small delay to simulate processing
        time.sleep(0.01)
        sock.send_json(resp)
    except KeyboardInterrupt:
        break
    except Exception as e:
        print("Error processing request:", e)
        try:
            sock.send_json({"status": "error", "error": str(e)})
        except Exception:
            pass

sock.close()
ctx.term()