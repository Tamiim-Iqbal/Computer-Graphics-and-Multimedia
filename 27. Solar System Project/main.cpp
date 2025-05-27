#define _Alignof alignof
#include "glad.h"
#include "glfw3.h"
#include "include/glm/glm/glm.hpp"
#include "include/glm/glm/gtc/matrix_transform.hpp"
#include "include/glm/glm/gtc/type_ptr.hpp"
#include <vector>
#include <iostream>
#include <cmath>

// Constants
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;
const int CIRCLE_SEGMENTS = 64;
const float TWO_PI = 2.0f * M_PI;

// Simulation controls
bool isPaused = false;
float timeSpeed = 0.005f;
float zoom = 1.0f;
double currentTime = 0.0;  // Use double for better precision

// Shader sources
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 projection;
uniform mat4 model;
void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main() {
    FragColor = vec4(color, 1.0);
}
)glsl";

// Callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    zoom -= yoffset * 0.05f;
    zoom = glm::clamp(zoom, 0.1f, 5.0f);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool spacePressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!spacePressed) {
            isPaused = !isPaused;
            spacePressed = true;
        }
    } else {
        spacePressed = false;
    }

    // Speed control with clamping
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        timeSpeed = glm::min(1.0f, timeSpeed + 0.001f);
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        timeSpeed = glm::max(0.001f, timeSpeed - 0.001f);
        
    // Reset simulation with R key
    static bool resetPressed = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (!resetPressed) {
            currentTime = 0.0;
            resetPressed = true;
        }
    } else {
        resetPressed = false;
    }
}

// Normalize angle to prevent overflow
float normalizeAngle(double angle) {
    // Keep angle in range [0, 2π] to prevent floating point precision issues
    while (angle >= TWO_PI) angle -= TWO_PI;
    while (angle < 0) angle += TWO_PI;
    return static_cast<float>(angle);
}

// Create circle vertices (optimized)
std::vector<float> createCircleVertices(float radius, bool filled) {
    std::vector<float> vertices;
    if (filled) {
        vertices.reserve((CIRCLE_SEGMENTS + 2) * 2);
        vertices.push_back(0.0f);  // Center point
        vertices.push_back(0.0f);
    } else {
        vertices.reserve((CIRCLE_SEGMENTS + 1) * 2);
    }

    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float angle = TWO_PI * i / CIRCLE_SEGMENTS;
        vertices.push_back(radius * cosf(angle));
        vertices.push_back(radius * sinf(angle));
    }

    return vertices;
}

// Setup VAO/VBO (optimized)
struct CircleVAO {
    unsigned int VAO, VBO;
    int vertexCount;
};

CircleVAO setupCircleVAO(const std::vector<float>& vertices) {
    CircleVAO circle;
    glGenVertexArrays(1, &circle.VAO);
    glGenBuffers(1, &circle.VBO);
    
    glBindVertexArray(circle.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, circle.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    circle.vertexCount = vertices.size() / 2;
    return circle;
}

// Shader compilation (with better error reporting)
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
        return 0;
    }
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    if (!vertexShader || !fragmentShader) return 0;
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error:\n" << infoLog << std::endl;
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Fixed Solar System", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Create shader program
    unsigned int shaderProgram = createShaderProgram();
    if (!shaderProgram) return -1;

    // Get uniform locations
    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int colorLoc = glGetUniformLocation(shaderProgram, "color");

    // Create circle geometries (optimized)
    auto filledCircle = createCircleVertices(1.0f, true);
    auto orbitCircle = createCircleVertices(1.0f, false);
    
    CircleVAO filledVAO = setupCircleVAO(filledCircle);
    CircleVAO orbitVAO = setupCircleVAO(orbitCircle);

    // Planet data (with individual angle tracking)
    struct Planet {
        float orbitRadius;
        float radius;
        float speed;
        glm::vec3 color;
        double currentAngle;  // Individual angle tracking with double precision
    };

    Planet planets[] = {
        {0.15f, 0.02f, 0.8f, {0.5f, 0.5f, 0.5f}, 0.0},    // Mercury
        {0.25f, 0.03f, 0.6f, {1.0f, 0.5f, 0.1f}, 0.0},    // Venus
        {0.35f, 0.035f, 0.4f, {0.1f, 0.6f, 1.0f}, 0.0},   // Earth
        {0.45f, 0.025f, 0.3f, {1.0f, 0.2f, 0.2f}, 0.0},   // Mars
        {0.6f, 0.04f, 0.2f, {0.9f, 0.5f, 0.1f}, 0.0},     // Jupiter
        {0.75f, 0.035f, 0.15f, {0.9f, 0.9f, 0.6f}, 0.0},  // Saturn
        {0.9f, 0.03f, 0.1f, {0.5f, 0.9f, 1.0f}, 0.0}      // Uranus
    };

    // For smooth time progression
    double lastTime = glfwGetTime();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        double frameTime = glfwGetTime();
        double deltaTime = frameTime - lastTime;
        lastTime = frameTime;

        processInput(window);
        
        if (!isPaused) {
            // Update each planet's angle individually with delta time
            for (auto& planet : planets) {
                planet.currentAngle += deltaTime * timeSpeed * planet.speed;
                // Normalize angle to prevent precision loss
                if (planet.currentAngle >= TWO_PI) {
                    planet.currentAngle -= TWO_PI;
                }
            }
        }

        // Clear screen
        glClearColor(0.0f, 0.0f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set shader and projection
        glUseProgram(shaderProgram);
        glm::mat4 projection = glm::ortho(-zoom, zoom, -zoom, zoom, -1.0f, 1.0f);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw orbits
        glBindVertexArray(orbitVAO.VAO);
        glUniform3f(colorLoc, 0.3f, 0.3f, 0.3f);
        for (const auto& planet : planets) {
            
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(planet.orbitRadius));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_LINE_LOOP, 0, orbitVAO.vertexCount);
        }

        // Draw sun
        glBindVertexArray(filledVAO.VAO);
        glm::mat4 sunModel = glm::scale(glm::mat4(1.0f), glm::vec3(0.08f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sunModel));
        glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, filledVAO.vertexCount);

        // Draw planets
        for (const auto& planet : planets) {
            float angle = static_cast<float>(planet.currentAngle);
            float x = planet.orbitRadius * cosf(angle);
            float y = planet.orbitRadius * sinf(angle);
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            model = glm::scale(model, glm::vec3(planet.radius));
            
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(colorLoc, 1, glm::value_ptr(planet.color));
            glDrawArrays(GL_TRIANGLE_FAN, 0, filledVAO.vertexCount);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &filledVAO.VAO);
    glDeleteBuffers(1, &filledVAO.VBO);
    glDeleteVertexArrays(1, &orbitVAO.VAO);
    glDeleteBuffers(1, &orbitVAO.VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}