import json

doc = '{"name": "bench", "n": 42, "ok": true, "tags": ["a", "b", "c"], "addr": {"city": "Zurich", "zip": 8000}}'
out = ""
for i in range(5000):
    j = json.loads(doc)
    out = json.dumps(j)
print("jsonlen=", len(out))
