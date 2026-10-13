#include "timeline-agent.h"

#include "agent-orchestrator.h"
#include "simulation-environment.h"
#include "traffic-app.h"

#include "ns3/simulator.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace ns3 {

TimelineAgent::TimelineAgent(const std::string& path, AgentOrchestrator* orchestrator)
    : m_env(nullptr), m_orchestrator(orchestrator), m_path(path) {}

void TimelineAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
    m_in.open(m_path);
    if (!m_in) {
        std::cerr << "[TIMELINE] cannot open " << m_path << std::endl;
        return;
    }
    Poll();
}

void TimelineAgent::Tick(double) { Poll(); }

// Reads every complete line not seen yet and schedules it at its timestamp.
void TimelineAgent::Poll() {
    if (!m_in.is_open()) return;
    std::string line;
    while (std::getline(m_in, line)) {
        if (m_in.eof()) {  // writer has not finished this line yet
            m_partial += line;
            break;
        }
        line = m_partial + line;
        m_partial.clear();
        std::istringstream ss(line);
        std::string when;
        if (!(ss >> when) || when[0] == '#') continue;
        double delay = 0.0;
        if (when != "now") {
            try {
                delay = std::max(0.0, std::stod(when) - Simulator::Now().GetSeconds());
            } catch (...) {
                std::cerr << "[TIMELINE] bad time: " << line << std::endl;
                continue;
            }
        }
        Simulator::Schedule(Seconds(delay), &TimelineAgent::Execute, this, line);
    }
    m_in.clear();
    m_in.seekg(0, std::ios::cur);  // seeking resets the stdio EOF flag so appended lines become visible
}

bool TimelineAgent::Expand(const std::string& target, std::vector<std::pair<bool, uint32_t>>& out) const {
    const uint32_t nSta = m_env->GetNumStas();
    const uint32_t nAp = m_env->GetNumAps();
    std::string kind = target, sel = "*";
    if (auto c = target.find(':'); c != std::string::npos) {
        kind = target.substr(0, c);
        sel = target.substr(c + 1);
    }
    if (target == "all") kind = "sta";
    if (kind != "sta" && kind != "ap" && kind != "cell") return false;

    const uint32_t n = kind == "sta" ? nSta : nAp;
    uint32_t lo = 0, hi = n == 0 ? 0 : n - 1;
    if (sel != "*") {
        try {
            auto dash = sel.find('-');
            lo = std::stoul(sel.substr(0, dash));
            hi = dash == std::string::npos ? lo : std::stoul(sel.substr(dash + 1));
        } catch (...) {
            return false;
        }
    }
    if (n == 0 || lo > hi || hi >= n) return false;

    for (uint32_t i = lo; i <= hi; ++i) {
        if (kind == "ap") {
            out.push_back({true, i});
        } else if (kind == "sta") {
            out.push_back({false, i});
        } else {
            for (uint32_t s = 0; s < nSta; ++s)
                if (m_env->GetStaCell(s) == i) out.push_back({false, s});
        }
    }
    return true;
}

void TimelineAgent::Execute(const std::string& line) {
    std::istringstream ss(line);
    std::string when, cmd, target, a1, a2, a3;
    ss >> when >> cmd >> target >> a1 >> a2 >> a3;
    const double now = Simulator::Now().GetSeconds();
    auto bad = [&](const char* why) { std::cerr << "[TIMELINE] " << why << ": " << line << std::endl; };
    std::cout << "[TIMELINE] t=" << now << " " << line.substr(line.find(cmd)) << std::endl;

    // Global commands (no node target)
    if (cmd == "STOP") {
        Simulator::Stop();
        return;
    }
    if (cmd == "MODE") {
        static const std::pair<const char*, SimulationMode> modes[] = {
            {"normal", SimulationMode::NORMAL_OPERATION},
            {"controlled", SimulationMode::CONTROLLED_LOAD},
            {"failure", SimulationMode::NETWORK_FAILURE},
            {"escalation", SimulationMode::STRESS_ESCALATION}};
        for (const auto& [name, mode] : modes) {
            if (target == name) {
                m_env->SetCurrentMode(mode);
                return;
            }
        }
        return bad("unknown mode");
    }
    if (cmd == "AGENT") {
        if (target == "timeline") return bad("timeline agent cannot be toggled");
        if (!m_orchestrator->SetAgentEnabled(target, a1 == "on")) bad("unknown agent");
        return;
    }

    std::vector<std::pair<bool, uint32_t>> nodes;
    if (!Expand(target, nodes)) return bad("bad target");

    for (const auto& [isAp, idx] : nodes) {
        if (cmd == "NODE") {
            if (a1 == "up" || a1 == "down") {
                m_env->SetNodeUp(isAp, idx, a1 == "up");
            } else if (a1 == "restart") {
                double downtime = 1.0;
                try {
                    if (!a2.empty()) downtime = std::stod(a2);
                } catch (...) {
                    return bad("NODE restart expects <downtime_s>");
                }
                m_env->SetNodeUp(isAp, idx, false);
                Simulator::Schedule(Seconds(downtime), &SimulationEnvironment::SetNodeUp, m_env, isAp, idx, true);
            } else {
                return bad("NODE expects up|down|restart");
            }
            continue;
        }

        if (isAp) return bad("command applies to STAs only (APs only receive)");
        Ptr<TrafficApp> app = m_env->GetStaApp(idx);
        if (!app) return bad("STA has no TrafficApp");

        if (cmd == "APP") {
            app->SetAppOn(a1 == "on");
            m_env->LogEvent(a1 == "on" ? "APP_ON" : "APP_OFF", 0, m_env->GetStaNodeId(idx));
        } else if (cmd == "TRAFFIC") {
            for (const std::string& kv : {a1, a2}) {
                Dist d;
                auto eq = kv.find('=');
                if (kv.empty()) continue;
                if (eq == std::string::npos || !Dist::Parse(kv.substr(eq + 1), d)) return bad("bad distribution");
                if (kv.compare(0, eq, "size") == 0) app->SetSize(d);
                else if (kv.compare(0, eq, "interval") == 0) app->SetInterval(d);
                else return bad("TRAFFIC expects size=/interval=");
            }
        } else if (cmd == "BURST") {
            try {
                app->Burst(std::stoul(a1), std::stod(a2));
            } catch (...) {
                return bad("BURST expects <count> <gap_us>");
            }
        } else if (cmd == "NETEM") {
            try {
                app->SetNetem(std::stod(a1), std::stod(a2), std::stod(a3));
            } catch (...) {
                return bad("NETEM expects <delay_ms> <jitter_ms> <loss>");
            }
        } else if (cmd == "ISOLATE") {
            app->SetIsolated(a1 == "on");
            m_env->LogEvent(a1 == "on" ? "ISOLATED" : "UNISOLATED", 0, m_env->GetStaNodeId(idx));
        } else {
            return bad("unknown command");
        }
    }
}

} // namespace ns3
