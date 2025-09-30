#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <stb_image.h>

#include <cfloat>
#include <iostream>
#include <vector>

// window settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// camera
Camera camera(glm::vec3(0.0f, 1.8f, 8.0f));
float lastX = static_cast<float>(SCR_WIDTH) / 2.0f;
float lastY = static_cast<float>(SCR_HEIGHT) / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// animation state
bool animationPaused = false;
float animationSpeed = 1.0f;
float animationTime = 0.0f;
static bool spacePressedLast = false;
static bool upPressedLast = false;
static bool downPressedLast = false;
static bool resetPressedLast = false;

// car bounds (model space)
glm::vec3 boundsMin(0.0f);
glm::vec3 boundsMax(0.0f);
glm::vec3 boundsCenter(0.0f);

struct MeshAnimationData
{
  glm::vec3 center{0.0f};
  glm::vec3 direction{0.0f};
  glm::vec3 rotationAxis{0.0f};
  float rotationAmount{0.0f};
  float phaseOffset{0.0f};
  float travelScale{1.0f};
};

std::vector<MeshAnimationData> meshAnimationData;

// function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

void computeModelBounds(const Model &model)
{
  glm::vec3 minBound(FLT_MAX);
  glm::vec3 maxBound(-FLT_MAX);

  for (const Mesh &mesh : model.meshes)
  {
    for (const Vertex &vertex : mesh.vertices)
    {
      minBound = glm::min(minBound, vertex.Position);
      maxBound = glm::max(maxBound, vertex.Position);
    }
  }

  boundsMin = minBound;
  boundsMax = maxBound;
  boundsCenter = (minBound + maxBound) * 0.5f;
}

float random01(const glm::vec3 &p, float seed)
{
  return glm::fract(glm::sin(glm::dot(p + glm::vec3(seed), glm::vec3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

glm::vec3 randomDirection(const glm::vec3 &p, float seed)
{
  glm::vec3 v(
      random01(p, seed + 0.123f) * 2.0f - 1.0f,
      random01(p, seed + 4.321f) * 2.0f - 1.0f,
      random01(p, seed + 8.765f) * 2.0f - 1.0f);
  if (glm::length(v) < 1e-3f)
    return glm::vec3(0.0f, 1.0f, 0.0f);
  return glm::normalize(v);
}

void computeMeshAnimationData(const Model &model)
{
  meshAnimationData.clear();
  meshAnimationData.reserve(model.meshes.size());

  for (const Mesh &mesh : model.meshes)
  {
    glm::vec3 minBound(FLT_MAX);
    glm::vec3 maxBound(-FLT_MAX);
    for (const Vertex &vertex : mesh.vertices)
    {
      minBound = glm::min(minBound, vertex.Position);
      maxBound = glm::max(maxBound, vertex.Position);
    }

    glm::vec3 center = (minBound + maxBound) * 0.5f;
    glm::vec3 direction = center - boundsCenter;
    if (glm::length(direction) < 1e-3f)
      direction = randomDirection(center, 2.57f);
    else
      direction = glm::normalize(direction);

    glm::vec3 rotationAxis = glm::normalize(glm::cross(direction, randomDirection(center, 9.31f)));
    if (glm::length(rotationAxis) < 1e-3f)
      rotationAxis = randomDirection(center, 15.73f);

    float travelScale = glm::mix(0.7f, 1.45f, random01(center, 3.71f));
    float rotationAmount = glm::mix(0.2f, 1.05f, random01(center, 6.42f));
    float phaseOffset = (random01(center, 9.88f) - 0.5f) * 1.2f;

    meshAnimationData.push_back({center, direction, rotationAxis, rotationAmount, phaseOffset, travelScale});
  }
}

int main()
{
  // glfw init
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "HW3: McLaren Assembly", nullptr, nullptr);
  if (window == nullptr)
  {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  glEnable(GL_DEPTH_TEST);

  Shader carShader("car_animation.vs", "car_animation.fs");
  Shader lampShader("lamp.vs", "lamp.fs");

  stbi_set_flip_vertically_on_load(false);
  Model carModel(FileSystem::getPath("src/hw3/model/2025_mclaren_artura_spider/scene.gltf"));
  std::cout << "Model directory: " << carModel.directory << std::endl;
  computeModelBounds(carModel);
  computeMeshAnimationData(carModel);

  // lamp cube geometry
  float lampVertices[] = {
      -0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, 0.5f, -0.5f,
      0.5f, 0.5f, -0.5f,
      -0.5f, 0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f,

      -0.5f, -0.5f, 0.5f,
      0.5f, -0.5f, 0.5f,
      0.5f, 0.5f, 0.5f,
      0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f,
      -0.5f, -0.5f, 0.5f,

      -0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f,

      0.5f, 0.5f, 0.5f,
      0.5f, 0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, 0.5f,
      0.5f, 0.5f, 0.5f,

      -0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, 0.5f,
      0.5f, -0.5f, 0.5f,
      -0.5f, -0.5f, 0.5f,
      -0.5f, -0.5f, -0.5f,

      -0.5f, 0.5f, -0.5f,
      0.5f, 0.5f, -0.5f,
      0.5f, 0.5f, 0.5f,
      0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, -0.5f};

  unsigned int lampVAO = 0, lampVBO = 0;
  glGenVertexArrays(1, &lampVAO);
  glGenBuffers(1, &lampVBO);

  glBindVertexArray(lampVAO);
  glBindBuffer(GL_ARRAY_BUFFER, lampVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(lampVertices), lampVertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  const glm::vec3 lampColors[4] = {
      glm::vec3(1.0f, 0.7f, 0.3f),
      glm::vec3(0.6f, 0.8f, 1.0f),
      glm::vec3(0.9f, 0.4f, 0.8f),
      glm::vec3(0.6f, 1.0f, 0.6f)};

  // render loop
  while (!glfwWindowShouldClose(window))
  {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);
    if (!animationPaused)
      animationTime += deltaTime * animationSpeed;

    glClearColor(0.018f, 0.018f, 0.032f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT), 0.1f, 200.0f);
    glm::mat4 view = camera.GetViewMatrix();

    // animate light positions
    glm::vec3 pointLightPositions[4];
    pointLightPositions[0] = glm::vec3(2.8f * sin(animationTime * 0.45f), 2.4f, 2.8f * cos(animationTime * 0.45f));
    pointLightPositions[1] = glm::vec3(-3.0f * sin(animationTime * 0.32f + glm::pi<float>() * 0.33f), 1.6f + 0.2f * sin(animationTime * 1.5f), 3.0f * cos(animationTime * 0.32f + glm::pi<float>() * 0.33f));
    pointLightPositions[2] = glm::vec3(0.0f, 3.6f + 0.5f * sin(animationTime * 0.9f), 3.4f);
    pointLightPositions[3] = glm::vec3(0.0f, 1.3f + 0.4f * sin(animationTime * 1.3f), -3.6f - 0.8f * sin(animationTime * 0.8f));

    carShader.use();
    carShader.setMat4("projection", projection);
    carShader.setMat4("view", view);
    carShader.setVec3("viewPos", camera.Position);
    carShader.setFloat("time", animationTime);
    carShader.setFloat("disassembleDistance", 5.6f);
    carShader.setFloat("ambientStrength", 0.18f);
    carShader.setFloat("materialShininess", 64.0f);

    // directional light
    carShader.setVec3("dirLight.direction", glm::vec3(-0.35f, -1.0f, -0.4f));
    carShader.setVec3("dirLight.ambient", glm::vec3(0.12f));
    carShader.setVec3("dirLight.diffuse", glm::vec3(0.35f, 0.35f, 0.4f));
    carShader.setVec3("dirLight.specular", glm::vec3(0.45f));

    for (int i = 0; i < 4; ++i)
    {
      std::string base = "pointLights[" + std::to_string(i) + "]";
      carShader.setVec3(base + ".position", pointLightPositions[i]);
      carShader.setVec3(base + ".ambient", lampColors[i] * 0.12f);
      carShader.setVec3(base + ".diffuse", lampColors[i] * 0.9f);
      carShader.setVec3(base + ".specular", glm::vec3(1.0f));
      carShader.setFloat(base + ".constant", 1.0f);
      carShader.setFloat(base + ".linear", 0.045f);
      carShader.setFloat(base + ".quadratic", 0.0075f);
    }

    carShader.setVec3("spotLight.position", camera.Position);
    carShader.setVec3("spotLight.direction", camera.Front);
    carShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
    carShader.setVec3("spotLight.diffuse", glm::vec3(0.85f));
    carShader.setVec3("spotLight.specular", glm::vec3(1.0f));
    carShader.setFloat("spotLight.constant", 1.0f);
    carShader.setFloat("spotLight.linear", 0.045f);
    carShader.setFloat("spotLight.quadratic", 0.010f);
    carShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(13.0f)));
    carShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -0.8f, 0.0f));
    model = glm::scale(model, glm::vec3(1.0f));
    carShader.setMat4("model", model);

    if (meshAnimationData.size() != carModel.meshes.size())
      computeMeshAnimationData(carModel);

    for (size_t i = 0; i < carModel.meshes.size(); ++i)
    {
      const MeshAnimationData &anim = meshAnimationData[i];
      carShader.setVec3("meshCenter", anim.center);
      carShader.setVec3("meshDirection", anim.direction);
      carShader.setVec3("meshRotationAxis", anim.rotationAxis);
      carShader.setFloat("meshRotationAmount", anim.rotationAmount);
      carShader.setFloat("meshPhaseOffset", anim.phaseOffset);
      carShader.setFloat("meshTravelScale", anim.travelScale);
      carModel.meshes[i].Draw(carShader);
    }

    lampShader.use();
    lampShader.setMat4("projection", projection);
    lampShader.setMat4("view", view);
    glBindVertexArray(lampVAO);
    for (int i = 0; i < 4; ++i)
    {
      glm::mat4 lampModel = glm::mat4(1.0f);
      lampModel = glm::translate(lampModel, pointLightPositions[i]);
      lampModel = glm::scale(lampModel, glm::vec3(0.15f));
      lampShader.setMat4("model", lampModel);
      lampShader.setVec3("lightColor", lampColors[i]);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &lampVAO);
  glDeleteBuffers(1, &lampVBO);

  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window)
{
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  float speedScale = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 2.5f : 1.0f;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime * speedScale);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime * speedScale);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime * speedScale);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime * speedScale);
  float verticalVelocity = camera.MovementSpeed * deltaTime * speedScale;
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.Position -= camera.Up * verticalVelocity;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.Position += camera.Up * verticalVelocity;

  bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
  if (spacePressed && !spacePressedLast)
    animationPaused = !animationPaused;
  spacePressedLast = spacePressed;

  bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
  if (upPressed && !upPressedLast)
    animationSpeed = glm::clamp(animationSpeed + 0.25f, 0.25f, 5.0f);
  upPressedLast = upPressed;

  bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
  if (downPressed && !downPressedLast)
    animationSpeed = glm::clamp(animationSpeed - 0.25f, 0.25f, 5.0f);
  downPressedLast = downPressed;

  bool resetPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
  if (resetPressed && !resetPressedLast)
  {
    animationSpeed = 1.0f;
    animationTime = 0.0f;
    animationPaused = false;
  }
  resetPressedLast = resetPressed;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (firstMouse)
  {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos; // reversed due to y-axis direction

  lastX = xpos;
  lastY = ypos;

  camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double /*xoffset*/, double yoffset)
{
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
