#include "simulate.hpp"
#include <iostream>
#include "methods/progressive.hpp"

#include <list>

namespace pdc {

namespace {

struct CallPoint {
    Call call;
    double timestamp;
};

struct LastCallTime {
    double time_passed = 0.f;
    double overtime = 0.f;
};

std::size_t toNewCalls (const MethodResult& result, LastCallTime& call_time) {
    if (result.is_rate) {
        double time_passed = call_time.time_passed + call_time.overtime;
        //std::cout << "time_passed:" << call_time.time_passed << " + overtime:" << call_time.overtime << " = " << time_passed << "\n";
        if (time_passed && result.rate * time_passed > 1) {
            call_time.overtime = time_passed - static_cast<std::size_t>(time_passed * result.rate) / result.rate;
            return static_cast<std::size_t>(time_passed * result.rate);
        }
        return 0u;
    }
    return result.calls;
}

std::size_t makeNewCalls (Method& method, const std::vector<Call>& calls, std::list<CallPoint>& call_points,
                          std::size_t& newCallIndex, std::size_t agents, LastCallTime & since_last_call) {
    std::vector<double> service_calls, setup_calls;
    for (const auto& call_point : call_points) {
        if (call_point.timestamp > call_point.call.setup)
            service_calls.push_back(call_point.timestamp - call_point.call.setup);
        else
            setup_calls.push_back(call_point.timestamp);
    }
    auto result = toNewCalls(method.calculate(agents, setup_calls, service_calls), since_last_call);
    std::size_t new_calls = result;

    //std::cout << "Make " << new_calls << "\n";

    for (std::size_t i = 0; i < new_calls; i++) {
        since_last_call.time_passed = 0; 
        if ( newCallIndex >= calls.size())
            break;
        Call newCall = calls[newCallIndex++];
        // std::cout << "MAKING NEW CALL: service:" << newCall.service << " setup:" << newCall.setup << " ans:" << newCall.answered << "\n";
        CallPoint newCallPoint = {newCall, 0};
        //std::cout << newCall.answered << " : " << newCall.setup << " : " << newCall.service << "\n";
        call_points.push_back(newCallPoint);
    }

    //std::cout << "Free " << agents << "\nsetup_calls:";
    //for (auto x : setup_calls)
        //std::cout << x << ", ";
    //std::cout << "\nservice_calls:";
    //for (auto x : service_calls)
        //std::cout << x << ", ";
    //std::cout << "\n\n";
    // std::cout << "SERVICE:" << service_calls.size() << " SETUP:" << setup_calls.size() << "\n";
    return agents - service_calls.size();
}

} // namespace

SimResult simulate (double step, std::size_t agents, Method& method, const std::vector<Call>& calls) {
    std::size_t newCallIndex = 0;
    std::list<CallPoint> call_points;
    SimResult result = {0, 0, 0, 0, 0, 0, 0};
    Progressive progressive;
    LastCallTime since_last_call {};
    while (true) {
        std::size_t free_agents = 0;
        if (result.finished && static_cast<double>(result.abandoned) / result.finished > 0.03) {
            result.limited_time += step;
            free_agents = makeNewCalls(progressive, calls, call_points, newCallIndex, agents, since_last_call);
        } else {
            free_agents = makeNewCalls(method, calls, call_points, newCallIndex, agents, since_last_call);
        }

        if (newCallIndex >= calls.size() && call_points.empty())
            break ;

        for (auto it = call_points.begin(); it != call_points.end();) {
            auto& timestamp = it->timestamp;
            auto& setup = it->call.setup;
            auto& service = it->call.service;
            auto& answered = it->call.answered;
            if (timestamp <= setup) {
                timestamp += step;
                if (timestamp > setup) {
                    if (!answered) {
                        method.addBusy(it->call.setup); // busy call
                        it = call_points.erase(it);
                        result.not_answered++;
                        continue;
                    }
                    if (!free_agents) {
                        //method.addCall(it->call); // abandoned TODO: add method in Method class to use and consider abandoned call data
                        it = call_points.erase(it);
                        //std::cout << "ABANDON!\n";
                        result.abandoned++;
                        continue;
                    }
                    result.answered++;
                    free_agents--;
                }
                it++;
                continue ;
            }
            timestamp += step;
            if (timestamp > setup + service) {
                method.addCall(it->call); // successful end
                it = call_points.erase(it);
                result.finished++;
                free_agents++;
                continue;
            }
            it++;
        }
        // std::cout << "RESULT: " << free_agents << "\n - - - \n";
        since_last_call.time_passed += step;
        result.duration += step;
        result.wait_time += step * free_agents;

    }
    return result;
}

} // namespace pdc
