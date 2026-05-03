#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <skytri/filesystem.h>
#include <skytri/shader.h>
#include <skytri/camera.h>
#include <skytri/model.h>
#include <skytri/cloth.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH  = 1920;
const unsigned int SCR_HEIGHT = 1080;
const float        MODEL_SCALE = 0.2f;  // Scale of imported models

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH  / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool  firstMouse        = true;
bool  imguiCaptureMouse = false;

float deltaTime = 0.0f;  // Time between current frame and last frame, makes physics framerate-independent
float lastFrame = 0.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "SkyTri", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Framebuffer
    // Post-processing, instead of drawing to the default framebuffer, draw to two textures
    // colorTex and depthTex so the post-processing shader can read them and apply effects.
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    unsigned int colorTex;
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT,
                 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTex, 0);

    unsigned int depthTex;
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Two triangles to cover the whole screen, in the second render pass this quad is
    // drawn with the post shader which samples the FBO textures and outputs the final processed image.
    float quadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          (void*)(2*sizeof(float)));
    glBindVertexArray(0);

    Shader postShader ("shaders/post.vert",          "shaders/post.frag");
    Shader ourShader  ("shaders/model_loading.vert", "shaders/model_loading.frag");
    Shader clothShader("shaders/cloth.vert",         "shaders/cloth.frag");

    glEnable(GL_DEPTH_TEST);
    
    Model ourModel(FileSystem::getPath("assets/models/firion/FirionEdited.obj"));

    // Finds the highest Y value among the cloth vertives and pins those within pinYTolerance it.
    float pinYTolerance = 0.15f;

    Cloth cape = Cloth::loadFromOBJ(
        FileSystem::getPath("assets/models/cape/Cape3.obj"),
        pinYTolerance
    );

    WindField wind;
    wind.direction  = glm::vec3(0.1f, 0.0f, -1.0f);
    wind.strength   = 1.0f;
    wind.turbulence = 4.0f;

    // ── Capsule colliders ────────────────────────────────────────────────────
    // a = top endpoint, b = bottom endpoint, both in world space.
    // Can adjust in ImGui and alter here.
    std::vector<CapsuleCollider> colliders;

    CapsuleCollider torso;
    torso.a = glm::vec3(-0.05f, 1.46f,  -0.07f);   // top of head
    torso.b = glm::vec3(-0.06f, 0.96f,  -0.10f);   // waist 
    torso.radius = 0.19f;        
    snprintf(torso.name, sizeof(torso.name), "Torso");
    colliders.push_back(torso);

    CapsuleCollider lArmUpper;
    lArmUpper.a = glm::vec3(-0.23f, 1.32f,  -0.09f);   // left shoulder
    lArmUpper.b = glm::vec3(-0.26f, 1.27f,  -0.09f);   // left elbow
    lArmUpper.radius = 0.10f;
    snprintf(lArmUpper.name, sizeof(lArmUpper.name), "L arm upper");
    colliders.push_back(lArmUpper);

    CapsuleCollider lArmLower;
    lArmLower.a = glm::vec3(-0.34f, 1.11f,  -0.03f);   // left elbow
    lArmLower.b = glm::vec3(-0.36f, 0.99f,  0.09f);   // left wrist
    lArmLower.radius = 0.09f;
    snprintf(lArmLower.name, sizeof(lArmLower.name), "L forearm");
    colliders.push_back(lArmLower);

    CapsuleCollider rArmUpper;
    rArmUpper.a = glm::vec3( 0.14f, 1.25f,  -0.21f);   // right shoulder
    rArmUpper.b = glm::vec3( 0.16f, 1.17f,  -0.24f);   // right elbow
    rArmUpper.radius = 0.11f;
    snprintf(rArmUpper.name, sizeof(rArmUpper.name), "R arm upper");
    colliders.push_back(rArmUpper);

    CapsuleCollider rArmLower;
    rArmLower.a = glm::vec3( 0.19f, 0.98f,  -0.20f);   // right elbow
    rArmLower.b = glm::vec3( 0.20f, 0.87f,  -0.12f);   // right wrist
    rArmLower.radius = 0.10f;
    snprintf(rArmLower.name, sizeof(rArmLower.name), "R forearm");
    colliders.push_back(rArmLower);

    CapsuleCollider legs;
    legs.a = glm::vec3(-0.05f, 0.69f,  -0.06f);   // hip height
    legs.b = glm::vec3(-0.01f, 0.29f,  -0.11f);   // ground height
    legs.radius = 0.21f;
    snprintf(legs.name, sizeof(legs.name), "Legs");
    colliders.push_back(legs);

    glm::vec3 characterPos = glm::vec3(0.0f);
    glm::vec3 capeOffset   = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 capeColor    = glm::vec3(0.8f, 0.9f, 1.0f);
    bool      showDebug    = true;
    bool      wireframe    = false;

    DebugCapsule debugCapsule;
    bool showColliders = true;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::min(currentFrame - lastFrame, 0.02f);
        lastFrame = currentFrame;

        processInput(window);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::GetIO().MouseDrawCursor = imguiCaptureMouse;

        if (showDebug)
        {
            ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(10,  10),   ImGuiCond_FirstUseEver);
            ImGui::Begin("Cloth debug", &showDebug);

            if (ImGui::BeginTabBar("tabs"))
            {
                // ── Cloth tab ────────────────────────────────────
                if (ImGui::BeginTabItem("Cloth"))
                {
                    ImGui::SeparatorText("Simulation");
                    ImGui::SliderFloat("Gravity Y",    &cape.gravity.y,   -20.f, 0.f,    "%.2f");
                    ImGui::SliderFloat("Damping",      &cape.damping,       0.8f, 1.f,   "%.3f");
                    ImGui::SliderFloat("Stretch limit",&cape.stretchLimit,  1.0f, 1.5f,  "%.2f");
                    ImGui::SliderFloat("Inertia scale",&cape.inertiaScale,  0.0f, 5.0f,  "%.1f");
                    ImGui::SliderFloat("Structural stiffness", &cape.structStiffness, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Bend stiffness", &cape.bendStiffness, 0.0f, 0.1f, "%.3f");

                    ImGui::SeparatorText("Transform");
                    ImGui::SliderFloat("Offset X", &capeOffset.x, -1.f, 1.f, "%.2f");
                    ImGui::SliderFloat("Offset Y", &capeOffset.y,  0.f, 3.f, "%.2f");
                    ImGui::SliderFloat("Offset Z", &capeOffset.z, -1.f, 1.f, "%.2f");

                    ImGui::SeparatorText("Pins");
                    // Show Y range so you can see what tolerance you need
                    float minY = 1e9f, maxY = -1e9f;
                    for (auto& p : cape.particles) {
                        minY = std::min(minY, p.restPosition.y);
                        maxY = std::max(maxY, p.restPosition.y);
                    }
                    ImGui::Text("Mesh Y: %.4f  to  %.4f", minY, maxY);
                    ImGui::Text("Threshold: %.4f", maxY - pinYTolerance);
                    if (ImGui::SliderFloat("Pin tolerance", &pinYTolerance,
                                           0.001f, 1.0f, "%.3f"))
                        cape.repinByHeight(pinYTolerance);
                    int pinCount = 0;
                    for (auto& p : cape.particles) if (p.pinned) pinCount++;
                    ImGui::Text("Pinned: %d / %d", pinCount,
                                (int)cape.particles.size());

                    ImGui::SeparatorText("Appearance");
                    ImGui::ColorEdit3("Color", &capeColor.x);
                    ImGui::Checkbox("Wireframe", &wireframe);

                    ImGui::SeparatorText("Info");
                    ImGui::Text("Particles: %d  Springs: %d",
                                (int)cape.particles.size(),
                                (int)cape.springs.size());
                    ImGui::Text("Substeps: %d  Iters: %d",
                                cape.substeps, Cloth::SOLVER_ITERS);
                    ImGui::EndTabItem();
                }

                // ── Colliders tab ────────────────────────────────
                if (ImGui::BeginTabItem("Colliders"))
                {
                    ImGui::Checkbox("Show colliders", &showColliders);
                    ImGui::Spacing();

                    for (int i = 0; i < (int)colliders.size(); i++)
                    {
                        auto& c = colliders[i];
                        ImGui::PushID(i);
                        if (ImGui::CollapsingHeader(c.name,
                                ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::SeparatorText("Top (a)");
                            ImGui::SliderFloat("aX", &c.a.x, -2.f, 2.f, "%.2f");
                            ImGui::SliderFloat("aY", &c.a.y,  0.f, 3.f, "%.2f");
                            ImGui::SliderFloat("aZ", &c.a.z, -2.f, 2.f, "%.2f");
                            ImGui::SeparatorText("Bottom (b)");
                            ImGui::SliderFloat("bX", &c.b.x, -2.f, 2.f, "%.2f");
                            ImGui::SliderFloat("bY", &c.b.y,  0.f, 3.f, "%.2f");
                            ImGui::SliderFloat("bZ", &c.b.z, -2.f, 2.f, "%.2f");
                            ImGui::SeparatorText("Shape");
                            ImGui::SliderFloat("Radius", &c.radius, 0.02f, 0.5f, "%.2f");

                            if (ImGui::SmallButton("Copy"))
                            {
                                char buf[256];
                                snprintf(buf, sizeof(buf),
                                    "a={%.2ff,%.2ff,%.2ff} b={%.2ff,%.2ff,%.2ff} r=%.2ff",
                                    c.a.x,c.a.y,c.a.z,
                                    c.b.x,c.b.y,c.b.z,
                                    c.radius);
                                ImGui::SetClipboardText(buf);
                            }
                            ImGui::SameLine();
                            ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.5f,0.1f,0.1f,1.f));
                            if (ImGui::SmallButton("Remove"))
                                colliders.erase(colliders.begin() + i);
                            ImGui::PopStyleColor();
                        }
                        ImGui::PopID();
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("+ Add capsule", ImVec2(-1, 0)))
                    {
                        CapsuleCollider c;
                        c.a = glm::vec3(0.f, 1.5f, 0.f);
                        c.b = glm::vec3(0.f, 1.0f, 0.f);
                        c.radius = 0.12f;
                        snprintf(c.name, sizeof(c.name),
                                 "Capsule %d", (int)colliders.size());
                        colliders.push_back(c);
                    }
                    ImGui::EndTabItem();
                }

                // ── Wind tab ─────────────────────────────────────
                if (ImGui::BeginTabItem("Wind"))
                {
                    ImGui::SeparatorText("Direction");
                    ImGui::SliderFloat("X##w", &wind.direction.x, -2.f, 2.f, "%.2f");
                    ImGui::SliderFloat("Y##w", &wind.direction.y, -2.f, 2.f, "%.2f");
                    ImGui::SliderFloat("Z##w", &wind.direction.z, -2.f, 2.f, "%.2f");
                    ImGui::SeparatorText("Properties");
                    ImGui::SliderFloat("Strength",   &wind.strength,   0.f, 100.f,  "%.1f");
                    ImGui::SliderFloat("Turbulence", &wind.turbulence,  0.f, 10.f, "%.1f");
                    if (ImGui::Button("Copy wind"))
                    {
                        char buf[128];
                        snprintf(buf, sizeof(buf),
                            "wind={{%.2ff,%.2ff,%.2ff},%.1ff,%.1ff};",
                            wind.direction.x, wind.direction.y, wind.direction.z,
                            wind.strength, wind.turbulence);
                        ImGui::SetClipboardText(buf);
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        // Cape model built here and passed to update and the shader so they both agree on where the cape is.
        glm::mat4 capeModel = glm::translate(glm::mat4(1.0f), capeOffset);
        capeModel = glm::scale(capeModel, glm::vec3(MODEL_SCALE));
        cape.update(deltaTime, wind, colliders, capeModel, characterPos);

        // First pass: scene to FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                               (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Draw the character model
        ourShader.use();
        ourShader.setVec3("lightPos", glm::vec3(5, 5, 5));
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model",
            glm::scale(glm::mat4(1.0f), glm::vec3(MODEL_SCALE)));
        ourModel.Draw(ourShader);

        // Draw the cape
        clothShader.use();
        clothShader.setMat4("projection", projection);
        clothShader.setMat4("view", view);
        clothShader.setVec3("uColor", capeColor);
        clothShader.setMat4("model", capeModel);
        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cape.draw();
        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Draw colliders, just using the simple cloth shader currently.
        if (showColliders)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Render only edges for colliders
            clothShader.use();
            clothShader.setMat4("projection", projection);
            clothShader.setMat4("view", view);
            clothShader.setVec3("uColor", glm::vec3(1.f, 0.35f, 0.35f));
            for (auto& c : colliders)
            {
                clothShader.setMat4("model", DebugCapsule::makeMatrix(c));
                debugCapsule.draw();
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // restore regular fill mode
        }

        // Second pass: post-process, unbind FBO to ddraw to the screen.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        postShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        postShader.setInt("colorTex", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        postShader.setInt("depthTex", 1);
        postShader.setFloat("near", 0.1f);
        postShader.setFloat("far",  10.0f);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Render ImGui on top of everything
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Tab toggles mouse capture for camera control vs ImGui interaction.
    static bool tabWas = false;
    bool tabIs = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tabIs && !tabWas)
    {
        imguiCaptureMouse = !imguiCaptureMouse;
        glfwSetInputMode(window, GLFW_CURSOR,
            imguiCaptureMouse ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
    tabWas = tabIs;

    // Movement controls. Only move the camera if ImGui isn't using the mouse.
    if (!imguiCaptureMouse)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD,  deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT,     deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT,    deltaTime);
    }
}

void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

// Fires when mouse moves. Used for camera control, ignored when ImGui is using mouse.
void mouse_callback(GLFWwindow*, double x, double y)
{
    if (imguiCaptureMouse) return;
    float xp = static_cast<float>(x), yp = static_cast<float>(y);
    if (firstMouse) { lastX = xp; lastY = yp; firstMouse = false; }
    camera.ProcessMouseMovement(xp - lastX, lastY - yp);
    lastX = xp; lastY = yp;
}

// Fires when scroll wheel moves. Used for camera control (zoom), ignored when ImGui is using mouse.
void scroll_callback(GLFWwindow*, double, double y)
{
    if (!imguiCaptureMouse)
        camera.ProcessMouseScroll(static_cast<float>(y));
}