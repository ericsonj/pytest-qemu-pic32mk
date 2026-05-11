class TestBootloaderRuns:

    def test_bootloader_runs(self, qemu):
        line = qemu.stdio.wait_for("Bootloader start", timeout=10)
        assert "Bootloader start" in line
