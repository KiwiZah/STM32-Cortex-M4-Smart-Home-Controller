"""Portable checks for the publicly shared archive's provenance and content.

These tests do NOT assert that STM32 hardware was retested or fully rebuilt.
"""
from pathlib import Path
import subprocess
import shutil
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RecoveryChecks(unittest.TestCase):
    def test_assembly_contains_actual_function(self):
        asm = (ROOT / 'firmware/reconstructed/password_check.s').read_text()
        self.assertIn('.global asm_check_password', asm)
        self.assertIn('ldrb r3, [r0, r2]', asm)
        self.assertIn('ldrb r4, [r1, r2]', asm)

    def test_reconstruction_marks_missing_original(self):
        txt = (ROOT/'firmware/reconstructed/main.c').read_text().lower()
        self.assertIn('reconstructed application main.c', txt)
        self.assertIn('asm_check_password(entered_pin, demo_pin)',txt)
        self.assertIn('bsp_lcd_glass_displaystring',txt)
        self.assertIn('tim4->ccr1',txt)
        self.assertIn('tim4->ccr2',txt)

    def test_blink_interrupt_uses_pa0(self):
        txt = (ROOT/'firmware/blink-test/main.c').read_text()
        self.assertIn('EXTI0_IRQHandler', txt)
        self.assertIn('EXTI_PR1', txt)
        self.assertIn('GPIOA_IDR', txt)

    @unittest.skipUnless(shutil.which('clang'), 'clang unavailable on host')
    def test_arm_assembly_compiles(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            result = subprocess.run([
                'clang', '-target', 'armv7em-none-eabi', '-mthumb', '-c',
                str(ROOT/'firmware/reconstructed/password_check.s'),
                '-o', str(Path(d)/'password_check.o')], text=True, capture_output=True)
            self.assertEqual(0, result.returncode, result.stderr)

if __name__=='__main__':
    unittest.main()
