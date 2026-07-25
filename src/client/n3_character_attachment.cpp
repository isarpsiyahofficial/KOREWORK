#include "client/n3_character_model.hpp"

namespace korework::client {

std::optional<content::N3Matrix4> N3CharacterModel::jointWorldMatrix(
    std::size_t jointIndex,
    float frame) const noexcept {
    if (!ready_) return std::nullopt;
    try {
        const auto matrices = skeleton_.worldMatrices(frame);
        if (jointIndex >= matrices.size()) return std::nullopt;
        return matrices[jointIndex];
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace korework::client
