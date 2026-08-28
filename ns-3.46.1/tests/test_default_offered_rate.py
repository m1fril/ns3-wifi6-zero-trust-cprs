import re
from pathlib import Path


def test_default_packet_interval_offers_ten_megabits_per_second():
    source = Path("scratch/network-project/network-project-main.cc").read_text()
    packet_size = int(re.search(r"uint32_t pSize = (\d+);", source).group(1))
    interval_us = int(re.search(r"uint32_t pktInterval = (\d+);", source).group(1))

    assert packet_size * 8 * 1_000_000 / interval_us == 10_000_000
