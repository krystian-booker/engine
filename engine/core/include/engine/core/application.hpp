#pragma once

// Application has moved to engine::app module.
// This header redirects for backwards compatibility.
#include <engine/app/application.hpp>

namespace engine::core {
    using Application = engine::app::Application;
}
