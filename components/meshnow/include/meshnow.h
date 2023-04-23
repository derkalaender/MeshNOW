#pragma once

#include <cstdint>
#include <memory>

class MeshNOWInternal;
class MeshNOW {
   public:
    /**
     * Initializes the MeshNOW library.
     *
     * This includes setting up WiFi and ESP-NOW.
     */
    MeshNOW();

    /**
     * Deinitializes the MeshNOW library.
     *
     * This includes shutting down WiFi and ESP-NOW.
     */
    ~MeshNOW();

   private:
    std::unique_ptr<MeshNOWInternal> internal;
};
