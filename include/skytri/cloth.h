#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>


struct ClothParticle {
    glm::vec3 position;
    glm::vec3 prevPosition;
    glm::vec3 velocity;
    bool pinned;
};

struct Spring {
    int a, b;
    float restLength;
};

class Cloth {
public:
    int width, height;
    std::vector<ClothParticle> particles;
    std::vector<Spring> springs;
    std::vector<unsigned int> indices;

    unsigned int VAO, VBO, EBO;

    float stiffness   = 2000.0f;
    float damping = 0.99f; 
    glm::vec3 gravity = glm::vec3(0.0f, -9.8f, 0.0f);
    glm::vec3 wind = glm::vec3(1.5f, 0.0f, 0.5f); // direction and strength
    float windTurbulence = 0.5f; // random gusting amount

    // ── Perf knobs (tune these) ──────────────────────────
    static constexpr int   SUBSTEPS        = 4;   // was 8
    static constexpr int   SOLVER_ITERS    = 3;   // was 5  →  12 passes total vs 40
    std::vector<glm::vec3> positionBuffer;

    // width/height = number of vertices along each axis
    // pinTop = pin the entire top row (for a hanging cape)
    Cloth(int width, int height, float spacing, bool pinTop = true)
        : width(width), height(height)
    {
        // Create particles
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width;  x++)
        {
            ClothParticle p;
            p.position     = glm::vec3(x * spacing, -y * spacing, 0.0f);
            p.prevPosition = p.position;
            p.velocity     = glm::vec3(0.0f);
            p.pinned       = pinTop && (y == 0);
            particles.push_back(p);
        }

        positionBuffer.resize(particles.size());

        // Structural springs (horizontal + vertical)
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width;  x++)
        {
            if (x + 1 < width)  addSpring(idx(x,y), idx(x+1,y));
            if (y + 1 < height) addSpring(idx(x,y), idx(x,y+1));
        }

        // Shear springs (diagonal)
        for (int y = 0; y < height - 1; y++)
        for (int x = 0; x < width  - 1; x++)
        {
            addSpring(idx(x,y),   idx(x+1,y+1));
            addSpring(idx(x+1,y), idx(x,  y+1));
        }

        // Bend springs (skip one vertex)
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width;  x++)
        {
            if (x + 2 < width)  addSpring(idx(x,y), idx(x+2,y));
            if (y + 2 < height) addSpring(idx(x,y), idx(x,y+2));
        }

        // Indices for rendering (two triangles per quad)
        for (int y = 0; y < height - 1; y++)
        for (int x = 0; x < width  - 1; x++)
        {
            indices.push_back(idx(x,   y));
            indices.push_back(idx(x+1, y));
            indices.push_back(idx(x,   y+1));
            indices.push_back(idx(x+1, y));
            indices.push_back(idx(x+1, y+1));
            indices.push_back(idx(x,   y+1));
        }

        setupBuffers();
    }

    void update(float dt)
    {
        float subDt = dt / SUBSTEPS;
        for (int s = 0; s < SUBSTEPS; s++)
        {
            applyForces(subDt);
            satisfyConstraints();
        }
        for (auto& p : particles)
            if (!p.pinned) p.velocity *= damping;

        uploadToGPU();
    }

    void draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    int idx(int x, int y) { return y * width + x; }

    void addSpring(int a, int b)
    {
        Spring s;
        s.a = a;
        s.b = b;
        s.restLength = glm::length(particles[a].position - particles[b].position);
        springs.push_back(s);
    }

    void applyWindToTriangle(int i0, int i1, int i2, float dt)
    {
        ClothParticle& p0 = particles[i0];
        ClothParticle& p1 = particles[i1];
        ClothParticle& p2 = particles[i2];

        // Face normal
        glm::vec3 edge1 = p1.position - p0.position;
        glm::vec3 edge2 = p2.position - p0.position;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        // Turbulence — simple time-based noise using sin
        float turbulence = 1.0f + windTurbulence * 
            sin(glfwGetTime() * 2.3f + p0.position.x * 3.7f) *
            cos(glfwGetTime() * 1.7f + p0.position.y * 2.9f);

        glm::vec3 windForce = wind * turbulence;

        // How much wind hits this face (dot product with normal)
        float windDot = glm::dot(normal, glm::normalize(windForce));
        glm::vec3 force = normal * windDot * glm::length(windForce) * dt / 3.0f;

        if (!p0.pinned) p0.velocity += force;
        if (!p1.pinned) p1.velocity += force;
        if (!p2.pinned) p2.velocity += force;
    }

    void applyForces(float dt)
    {
        // Apply gravity to all particles
        for (auto& p : particles)
        {
            if (p.pinned) continue;
            p.velocity += gravity * dt;
        }

        // Apply wind per triangle face
        for (int y = 0; y < height - 1; y++)
        for (int x = 0; x < width  - 1; x++)
        {
            // Get the two triangles of this quad
            int i0 = idx(x,   y);
            int i1 = idx(x+1, y);
            int i2 = idx(x,   y+1);
            int i3 = idx(x+1, y+1);

            applyWindToTriangle(i0, i1, i2, dt);
            applyWindToTriangle(i1, i3, i2, dt);
        }

        // Update positions
        for (auto& p : particles)
        {
            if (p.pinned) continue;
            p.position += p.velocity * dt;
        }
    }

    void satisfyConstraints()
    {
        for (int iter = 0; iter < SOLVER_ITERS; iter++)
        for (auto& s : springs)
        {
            ClothParticle& a = particles[s.a];
            ClothParticle& b = particles[s.b];

            glm::vec3 delta = b.position - a.position;
            float dist = glm::length(delta);
            if (dist == 0.0f) continue;

            float correction = (dist - s.restLength) / dist * 0.5f;
            glm::vec3 move = delta * correction;

            if (!a.pinned) a.position += move;
            if (!b.pinned) b.position -= move;
        }
    }

    void setupBuffers()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        glBindVertexArray(0);
        uploadToGPU();
    }

    void uploadToGPU()
    {
        for (size_t i = 0; i < particles.size(); i++)
            positionBuffer[i] = particles[i].position;

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            positionBuffer.size() * sizeof(glm::vec3),
            positionBuffer.data());
    }
};