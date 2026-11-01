import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
UNIT = ROOT / "docs/tools/music-rig/packaging/linux/music-rig-arturia-profile-session.service"


class ProfileSessionRestartPolicyTest(unittest.TestCase):
    def test_selector_restarts_after_failure_with_a_bounded_retry_window(self):
        service = UNIT.read_text(encoding="utf-8")

        self.assertIn("StartLimitIntervalSec=180s", service)
        self.assertIn("StartLimitBurst=3", service)
        self.assertIn("Restart=on-failure", service)
        self.assertIn("RestartSec=10s", service)
        self.assertNotIn("Restart=no", service)


if __name__ == "__main__":
    unittest.main(verbosity=2)
