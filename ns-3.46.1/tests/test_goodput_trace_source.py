import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ENVIRONMENT_SOURCE = PROJECT_ROOT / "scratch/network-project/simulation-environment.cc"


def test_goodput_is_counted_at_packet_socket_server_not_wifi_phy():
    source = ENVIRONMENT_SOURCE.read_text()

    assert "server->TraceConnect(" in source
    # Match across the line break without pinning the exact indent, so
    # reformatting the C++ source does not break this test.
    assert re.search(r'"Rx",\s*"/NodeList/"', source)
    assert 'MakeCallback(&SimulationEnvironment::ServerRxTrace' in source
    assert "serverAddr.SetSingleDevice(apWifiDev->GetIfIndex());" in source
    assert "server->SetLocal(serverAddr);" in source
    assert 'WifiPhy/PhyRxEnd", MakeCallback(&SimulationEnvironment::ServerRxTrace' not in source
