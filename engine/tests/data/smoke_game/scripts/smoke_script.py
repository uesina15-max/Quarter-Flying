"""player_template_smoke 테스트용 스크립트. ScriptSystem이 Player에서 수명주기를 부르는지 로그로 확인한다."""


class SmokeScript:
    def __init__(self, entity):
        self.entity = entity
        self.updates = 0

    def on_create(self):
        print("[smoke_script] on_create", flush=True)

    def on_update(self, dt):
        self.updates += 1

    def on_destroy(self):
        print(f"[smoke_script] on_destroy after {self.updates} updates", flush=True)
