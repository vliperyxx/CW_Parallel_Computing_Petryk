import random
from locust import HttpUser, task, constant

SEARCH_PHRASES = ["one of the", "this is a", "very good", "waste of time", "to be a", "part of the", "end of the"]
class WebClient(HttpUser):
    wait_time = constant(1)
    @task
    def search_and_snippet(self):
        phrase = random.choice(SEARCH_PHRASES)

        response = self.client.post("/search", json={"query": phrase}, name="/search")

        if response.status_code != 200:
            return

        try:
            data = response.json()
        except:
            return

        if data.get("status") == "busy":
            return

        results = data.get("results", [])

        if results:
            first_file_index = results[0].get("index")
            self.client.post("/snippet", json={"index": first_file_index}, name="/snippet")