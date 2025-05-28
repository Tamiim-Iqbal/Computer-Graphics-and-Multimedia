#define _Alignof alignof
#include "glad.h"
#include "glfw3.h"
#include "include/glm/glm/glm.hpp"
#include "include/glm/glm/gtc/matrix_transform.hpp"
#include "include/glm/glm/gtc/type_ptr.hpp"
#include <vector>
#include <iostream>
#include <cmath>
using namespace std;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
//solar : scroll callback for zooming
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

//solar : orbits. Not used bresenham, bcz OpenGL doesn't deal with pixels like Bresenham — it's vertex-based. Orbits may scale or animate, which works best with vertex math
const int CIRCLE_SEGMENTS = 512;    // Controls the smoothness of the circle
const float TWO_PI = 2.0f * M_PI;   // Constant for 2π, used for angle calculations

// Simulation controls
bool isPaused = false;     // simulation is running or paused. False : planets will move.
float timeSpeed = 0.005f;  // how fast time progresses in the simulation.
float zoom = 1.0f;         // Controls the zoom level 

// Shader sources
const char *vertexShaderSource ="#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 model;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * model * vec4(aPos, 0.0, 1.0);\n"
    "}\0";

// Fragment sources
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 color;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(color, 1.0);\n"
    "}\n\0";

// solar : all the properties needed to simulate and draw a planet orbiting
struct Planet {
    float orbitRadius;      // Distance from the center (sun) to the planet’s orbit.
    float radius;           // Size of the planet (for drawing the circle).
    float speed;            // Angular speed (how fast the planet orbits the sun).
    float color[3];         // RGB color of the planet (values between 0.0 and 1.0).
    double currentAngle;    // current angle of the planet on its orbit (in radians).
};

std::vector<float> createCircleVertices(float radius, bool filled);  // Generates the vertex positions of a circle, either as: a filled circle (like a planet) or an unfilled loop (like an orbit path). A list of x, y float coordinates to be sent to the GPU.
unsigned int createShaderProgram();
unsigned int setupCircleVAO(const std::vector<float>& vertices, unsigned int& VBO);  // Sets up the Vertex Array Object (VAO) and Vertex Buffer Object (VBO) for the circle so it can be rendered by OpenGL.

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solar System Simulation", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Create shader program
    unsigned int shaderProgram = createShaderProgram();
    if (!shaderProgram) return -1;

    // solar : Get uniform locations
    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");  // A matrix for projecting 3D scene onto the 2D screen
    int modelLoc = glGetUniformLocation(shaderProgram, "model");  // matrix to transform individual objects (translation, rotation, scaling).
    int colorLoc = glGetUniformLocation(shaderProgram, "color");  // RGB color to render the object.

    // solar : Create circle geometries
    auto filledCircle = createCircleVertices(1.0f, true);    // planet
    auto orbitCircle = createCircleVertices(1.0f, false);    // orbit
    
    unsigned int filledVBO, orbitVBO;
    unsigned int filledVAO = setupCircleVAO(filledCircle, filledVBO);
    unsigned int orbitVAO = setupCircleVAO(orbitCircle, orbitVBO);

    // soalr : Planet data
    Planet planets[] = {
        //Orbit, Planet, Speed, Color, Starting angle
        {0.15f, 0.02f, 0.8f, {0.6f, 0.6f, 0.6f}, 0.0},    // Mercury - grayish
        {0.25f, 0.03f, 0.6f, {0.9f, 0.7f, 0.3f}, 0.0},    // Venus - yellowish pale
        {0.35f, 0.035f, 0.4f, {0.15f, 0.7f, 0.5f}, 0.0},  // Earth - more green with blue
        {0.45f, 0.025f, 0.3f, {0.8f, 0.3f, 0.2f}, 0.0},   // Mars - reddish
        {0.6f, 0.04f, 0.2f, {0.9f, 0.7f, 0.5f}, 0.0},     // Jupiter - beige/orange
        {0.75f, 0.035f, 0.15f, {0.95f, 0.9f, 0.7f}, 0.0}, // Saturn - pale yellow
        {0.9f, 0.03f, 0.1f, {0.5f, 0.8f, 0.9f}, 0.0}      // Uranus - light blue/cyan
    };

    double lastTime = glfwGetTime();

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        //uses : if frames render faster or slower, the movement remains smooth and proportionate to real time
        double frameTime = glfwGetTime();          // Get the current time (seconds since GLFW started)
        double deltaTime = frameTime - lastTime;   // Calculate time passed since last frame
        lastTime = frameTime;                      // Update lastTime to current time for next frame

        // input
        processInput(window);
        
        if (!isPaused) 
        {
            for (auto& planet : planets) 
            {
                planet.currentAngle += deltaTime * timeSpeed * planet.speed;  // the planet's current position along its orbit, measured in radians.
                if (planet.currentAngle >= TWO_PI) 
                {
                    planet.currentAngle -= TWO_PI;
                }
                //If the angle goes beyond a full circle (2π radians), subtract 2π to wrap it back around to zero, keeping the angle in the range [0, 2π)
            }
        }

        // render
        glClearColor(0.0f, 0.0f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        // solar
        float aspectRatio = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        glm::mat4 projection = glm::ortho(-zoom * aspectRatio, zoom * aspectRatio, -zoom, zoom, -1.0f, 1.0f);    // x : left, right, y : bottom, top, z: near plane, far plane  - Multiplying by aspectRatio keeps the horizontal and vertical scales proportional, avoiding distortion
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);

        // solar : Draw orbits
        glBindVertexArray(orbitVAO);
        glUniform3f(colorLoc, 0.3f, 0.3f, 0.3f);  // Set orbit color to a dim grey
        for (const auto& planet : planets) 
        {
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(planet.orbitRadius));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
            glDrawArrays(GL_LINE_LOOP, 0, orbitCircle.size() / 2);
        }

        // solar : Draw sun
        glBindVertexArray(filledVAO);
        glm::mat4 sunModel = glm::scale(glm::mat4(1.0f), glm::vec3(0.08f));  // Create model matrix that scales a unit circle down to radius 0.08 (sun size)
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &sunModel[0][0]); // Pass the model matrix to the shader
        glUniform3f(colorLoc, 1.0f, 0.9f, 0.0f);  //yellow
        glDrawArrays(GL_TRIANGLE_FAN, 0, filledCircle.size() / 2);  // The number of vertices = half the size of filledCircle (because each vertex has x,y)

        // solar : Draw planets
        for (const auto& planet : planets) {
            float angle = static_cast<float>(planet.currentAngle);
            float x = planet.orbitRadius * cos(angle);
            float y = planet.orbitRadius * sin(angle);
            // x and y are calculated using cosine and sine, giving the correct position on the circular path based on the current orbit angle.
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            model = glm::scale(model, glm::vec3(planet.radius));
            
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
            glUniform3fv(colorLoc, 1, planet.color);
            glDrawArrays(GL_TRIANGLE_FAN, 0, filledCircle.size() / 2);
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();   
    }

    // optional: de-allocate all resources
    glDeleteVertexArrays(1, &filledVAO);
    glDeleteBuffers(1, &filledVBO);
    glDeleteVertexArrays(1, &orbitVAO);
    glDeleteBuffers(1, &orbitVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

std::vector<float> createCircleVertices(float radius, bool filled) {
    std::vector<float> vertices;
    if (filled) 
    {
        vertices.reserve((CIRCLE_SEGMENTS + 2) * 2);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
    } 
    else 
    {
        vertices.reserve((CIRCLE_SEGMENTS + 1) * 2);
    }

    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float angle = TWO_PI * i / CIRCLE_SEGMENTS;
        vertices.push_back(radius * std::cos(angle));
        vertices.push_back(radius * std::sin(angle));
    }
    return vertices;
}

unsigned int setupCircleVAO(const std::vector<float>& vertices, unsigned int& VBO) 
{
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    return VAO;
}

unsigned int compileShader(unsigned int type, const char* source) 
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
        return 0;
    }
    return shader;
}

unsigned int createShaderProgram() 
{
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

void processInput(GLFWwindow *window) 
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool spacePressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
    {
        if (!spacePressed) 
        {
            isPaused = !isPaused;
            spacePressed = true;
        }
    } 
    else 
    {
        spacePressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        timeSpeed = std::min(3.0f, timeSpeed + 0.001f);
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        timeSpeed = std::max(0.001f, timeSpeed - 0.001f);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
    zoom -= yoffset * 0.05f;
    zoom = std::max(0.95f, std::min(2.0f, zoom));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}