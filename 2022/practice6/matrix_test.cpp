#include <iostream>
#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

void print3x3(const glm::mat3x3 &A) {
    for (int i = 0; i < glm::mat3x3::length(); ++i) {
        for (int j = 0; j < glm::mat3x3::length(); ++j) {
            std::cout << A[i][j] << " ";
        }
        std::cout << "\n";
    }
}

int main() {
    glm::mat3x3 A;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            std::cin >> A[i][j];
        }
    }

    glm::vec3 b;
    std::cin >> b[0] >> b[1] >> b[2];

    auto solve = (glm::identity<glm::mat3x3>() / A) * b;

    //assert(A * (glm::identity<glm::mat3x3>() / A) == glm::identity<glm::mat3x3>());
    //assert(A * solve == b);

    std::cout << "Solution:\n";
    for (int i = 0; i < 3; ++i)
        std::cout << solve[i] << " ";
    std::cout << "\n";

    std::cout << "Result:\n";
    auto my_result = A * solve;
    for (int i = 0; i < 3; ++i)
        std::cout << my_result[i] << " ";

    return 0;
}
