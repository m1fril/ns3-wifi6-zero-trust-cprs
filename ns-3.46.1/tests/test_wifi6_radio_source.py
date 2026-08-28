from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ENVIRONMENT_SOURCE = PROJECT_ROOT / "scratch/network-project/simulation-environment.cc"


def test_wifi6_30_sta_radio_uses_explicit_80mhz_ap_four_stream_sta_two_stream_configuration():
    source = ENVIRONMENT_SOURCE.read_text()

    assert "wifi.SetStandard(WIFI_STANDARD_80211ax)" in source
    assert 'apPhy.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"))' in source
    assert 'staPhy.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"))' in source
    assert 'apPhy.Set("Antennas", UintegerValue(4))' in source
    assert 'apPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(4))' in source
    assert 'apPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(4))' in source
    assert 'staPhy.Set("Antennas", UintegerValue(2))' in source
    assert 'staPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2))' in source
    assert 'staPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2))' in source
