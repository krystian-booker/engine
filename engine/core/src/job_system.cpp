#include <engine/core/job_system.hpp>

namespace engine::core {

void JobSystem::init(int /*num_threads*/) {
    // TODO: implement thread pool
}

void JobSystem::shutdown() {
    // TODO: implement
}

void JobSystem::submit(std::function<void()> /*job*/) {
    // TODO: implement
}

void JobSystem::wait_all() {
    // TODO: implement
}

} // namespace engine::core
