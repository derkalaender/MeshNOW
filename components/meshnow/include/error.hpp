#pragma once

#include <esp_err.h>

#include <concepts>
#include <exception>

/**
 * Wraps an esp_err_t in a C++ exception.
 */
class ESPException : public std::exception {
   public:
    explicit ESPException(const esp_err_t error) : error{error} {};

    [[nodiscard]] const char* what() const noexcept override { return esp_err_to_name(error); }

    /**
     * The underlying error code.
     */
    const esp_err_t error;
};

/**
 * Throws an exception if the error is not ESP_OK.
 * @tparam Ex the specific exception type to throw
 * @param error the error code produced by an ESP-IDF function
 */
template <typename Ex = ESPException>
    requires std::derived_from<Ex, ESPException>
inline void CHECK_THROW(const esp_err_t error) {
    if (error != ESP_OK) {
        throw Ex{error};
    }
}

namespace MeshNOW {
class NotStartedException : public ESPException {
   public:
    NotStartedException() : ESPException{ESP_ERR_INVALID_STATE} {};
};

class AlreadyStartedException : public ESPException {
   public:
    AlreadyStartedException() : ESPException{ESP_ERR_INVALID_STATE} {};
};

class PayloadTooLargeException : public ESPException {
   public:
    PayloadTooLargeException() : ESPException{ESP_ERR_INVALID_ARG} {};
};
}  // namespace MeshNOW