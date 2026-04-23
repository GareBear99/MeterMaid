from pathlib import Path

print("MeterMaid validation plan starter")
print("1. Render or collect reference WAV files")
print("2. Capture MeterMaid readings in host")
print("3. Capture reference meter readings")
print("4. Compute per-file deltas for LUFS and dBTP")
print("5. Fail release if deviation exceeds target tolerance")

report = Path("validation_report_template.md")
report.write_text("""# Validation Report Template\n\n| File | MeterMaid Int LUFS | Ref Int LUFS | Delta | MeterMaid dBTP | Ref dBTP | Delta |\n|------|--------------------|--------------|-------|----------------|----------|-------|\n""")
print(f"Wrote {report}")
