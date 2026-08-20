#include "alongtrack/wavevortex_extension.hpp"

#include "WaveVortexRuntime/WVExtensionCatalog.hpp"
#include "WaveVortexRuntime/WVRunner.hpp"

#include <iostream>
#include <memory>
#include <utility>

int main(int argc, char** argv) {
    namespace runtime = wavevortex::runtime;

    runtime::WVExtensionCatalogBuilder builder;
    auto status = runtime::addBuiltInExtensions(builder);
    if (status) {
        status = alongtrack::wavevortex_extension::
            registerAlongTrackExtensions(builder);
    }

    std::shared_ptr<const runtime::WVExtensionCatalog> catalog;
    if (status) {
        status = builder.freeze(catalog);
    }
    if (!status || !catalog) {
        std::cerr << "Unable to construct the AlongTrack WaveVortex "
                     "extension catalog";
        if (!status.message.empty()) {
            std::cerr << ": " << status.message;
        }
        std::cerr << '\n';
        return 2;
    }

    return runtime::runWaveVortex(argc, argv, std::move(catalog));
}
