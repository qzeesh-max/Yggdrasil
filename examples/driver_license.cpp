#include <yggdrasil/state_machine.hpp>
#include <string>
#include <unordered_map>
#include <iostream>

using namespace yggdrasil;

// A demonstration of a real-world state machine for a DMV Driver's License process.
// Showcases transitions, data initialization, reflection mapping, error handling,
// and state validations (including reverting from final states).
struct driver_license_fsm : state_machine {
    // 1. Define all possible states
    enum class state {
        uninited,
        applied,
        permit,
        licensed,
        suspended,
        revoked,
        rejected
    };

    // 2. Define all possible events
    enum class event {
        pass_written_test,
        pass_driving_test,
        issue_citation,
        pay_fines,
        commit_felony
    };

    state driver_license_fsm{};
    
    // 2. Zero-boilerplate initialization
    // Parameters in `initialize` matching these names will automatically populate them.
    std::string applicantName;
    uint32_t age{};

    // Init from the 'age' parameter passed to the initial handler
    [[=init_from<^^age>{}]]
    uint32_t age_at_application{};

    // Init with a literal value
    [[=init_val<0>{}]]
    uint32_t totalPoints{};

    // Transparent hashing for map lookups
    struct hasher {
        using is_transparent = std::true_type;
        size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
        size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
        size_t operator()(const char* s) const { return std::hash<std::string_view>{}(s); }
    };

    struct citation_data_t {
        std::string reason;
        int points{};
    };

    using citation_map_t = std::unordered_map<std::string, citation_data_t, hasher, std::equal_to<>>;
    citation_map_t citations;

    // 3. Topology & Endpoints
    // The initial transition handles the instantiation of the state machine.
    // By convention, `to_applied` defines what state `initialize()` places us in (the first parameter of any_of, e.g. state::applied)
    // and what states we can transition to *from* `applied`.
    [[=initial{}]]
    auto to_applied(std::string_view applicantName, uint32_t age) -> any_of<state::permit, state::rejected>;

    // We define endpoints detailing what states can be reached *from* the method name's state
    [[=on_error("Must have a permit to get licensed")]]
    auto to_permit() -> any_of<state::licensed, state::revoked>;

    [[=on_error("Cannot get licensed from current state")]]
    auto to_licensed() -> any_of<state::suspended, state::revoked, state::licensed>;
    
    [[=on_error("License is already suspended")]]
    auto to_suspended() -> any_of<state::licensed, state::revoked, state::suspended>;

    // Final states - cannot naturally transition out of this state once reached
    auto to_revoked() -> final;
    auto to_rejected() -> final;

    // 5. Event Handlers

    // Event: pass_written_test
    // Transitions to permit or rejected
    [[=transition(any_of<state::permit, state::rejected>{})]]
    on pass_written_test(uint32_t score) {
        if (score >= 80) {
            std::cout << "[Event] " << applicantName << " passed written test with score " << score << ".\n";
            return to(state::permit);
        }
        std::cout << "[Event] " << applicantName << " failed written test.\n";
        return to(state::rejected);
    }

    // Event: pass_driving_test
    // Transitions to licensed or rejected
    [[=transition(any_of<state::licensed, state::rejected>{})]]
    on pass_driving_test(uint32_t score) {
        if (score >= 75) {
            std::cout << "[Event] " << applicantName << " passed driving test! Fully licensed.\n";
            return to(state::licensed);
        }
        std::cout << "[Event] " << applicantName << " failed driving test.\n";
        return to(state::rejected);
    }

    // Event: issue_citation
    // Can map new citations. Re-evaluates suspension.
    [[=transition(any_of<state::licensed, state::suspended>{})]]
    [[=mapping<^^citations>{}]]
    [[=on_error("Citation ID already exists in the system")]]
    on issue_citation([[=storage_key{}]] std::string_view citation_id, std::string_view reason, int points) {
        std::cout << "[Event] " << applicantName << " received citation: " << reason << " (" << points << " pts).\n";
        totalPoints += points;
        
        if (totalPoints >= 12) {
            std::cout << "        -> Points >= 12. License SUSPENDED.\n";
            return to(state::suspended);
        }
        
        if (driver_license_fsm == state::suspended) {
            return to(state::suspended);
        }
        
        return to(state::licensed);
    }

    // Event: pay_fines
    // Restores a suspended or revoked license.
    [[=can_revert_final{}]] // This allows reverting even if in a final state (like revoked)
    [[=transition(state::licensed)]]
    on pay_fines() {
        std::cout << "[Event] " << applicantName << " paid fines. Citations cleared. License restored.\n";
        citations.clear();
        totalPoints = 0;
        return accepted; // Resolves to state::licensed due to single-target annotation
    }

    // Event: commit_felony
    // Immediately revokes license.
    [[=transition(state::revoked)]]
    on commit_felony() {
        std::cout << "[Event] " << applicantName << " committed a felony. License REVOKED.\n";
        return accepted; // Resolves to state::revoked
    }
};

// Helper for printing errors gracefully in the example
void check(const auto& res) {
    if (!res.has_value()) {
        std::cout << "[Error] Transition rejected: " << res.error() << "\n";
    }
}

int main() {
    // 1. Build the proxy object using Yggdrasil
    auto fsm = build_state_machine_type<driver_license_fsm>{};

    // 2. Initial initialization
    std::cout << "--- Applying for John Doe (Age 18) ---\n";
    fsm.initialize("John Doe", 18);
    std::cout << "Applicant: " << fsm.applicantName() << " (Age: " << fsm.age_at_application() << ")\n";
    std::cout << "State is now: applied\n";

    // 3. Testing Valid Transitions
    std::cout << "\n--- Taking Tests ---\n";
    check(fsm.pass_written_test(85));
    check(fsm.pass_driving_test(90));
    std::cout << "State is now: licensed\n";

    // 4. Data Mapping & Collections
    std::cout << "\n--- Driving Record ---\n";
    check(fsm.issue_citation("T-001", "Speeding", 4));
    check(fsm.issue_citation("T-002", "Running Red Light", 5));
    
    std::cout << "Total Points: " << fsm.totalPoints() << "\n";
    std::cout << "Total Citations: " << fsm.citations().size() << "\n";

    // Attempt to add a duplicate citation ID (Yggdrasil rejects before body is executed)
    std::cout << "\n--- Attempting Duplicate Citation ---\n";
    check(fsm.issue_citation("T-001", "Speeding again", 4));

    // 5. Automatic Transitions Based on Logic
    std::cout << "\n--- License Suspension ---\n";
    check(fsm.issue_citation("T-003", "Reckless Driving", 6)); // Crosses 12 point limit
    
    // License is suspended. Attempting to issue another citation will fail 
    // because issue_citation only accepts state::licensed
    std::cout << "\n--- Issuing Citation while Suspended ---\n";
    check(fsm.issue_citation("T-004", "Parking Violation", 1));

    // 6. Restoring State
    std::cout << "\n--- Paying Fines ---\n";
    check(fsm.pay_fines());
    std::cout << "Total Points after fine payment: " << fsm.totalPoints() << "\n";

    // 7. Final States
    std::cout << "\n--- Committing Felony ---\n";
    check(fsm.commit_felony()); // Moves to final state 'revoked'

    // 8. Reverting a Final State
    std::cout << "\n--- Paying Fines to Revert Final State ---\n";
    // pay_fines has [[=can_revert_final{}]] which overrides the final state restriction
    check(fsm.pay_fines());
    
    std::cout << "\nExample complete!\n";
    return 0;
}
