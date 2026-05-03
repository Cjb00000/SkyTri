#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Each vertex of the cape obj becomes on of these particles.
struct ClothParticle {
    glm::vec3 position;      // current position (world space)
    glm::vec3 prevPosition;  // previous frame position (world space) — used to calculate velocity
    glm::vec3 restPosition;  // where it started (pin system reads this)
    bool      pinned;        // if true, this particle is fixed in place and won't move
};

// Connects two particles.
struct Spring {
    int   a, b;         // indices of the two particles this spring connects
    float restLength;   // how long the spring wants to be (the simulation constantly tries to
                        // push/pull particles back to this length).
    bool isBend = false; // if true, this spring is a bend spring (connects particles 2 apart in the grid)
};

// Represents a capsule-shaped collider, collision math finds the closest point on the 
// line segment from a and b to each particle, then pushes the particle out if it's inside the radius.
struct CapsuleCollider {
    glm::vec3 a;        // top endpoint (world space)
    glm::vec3 b;        // bottom endpoint (world space)
    float     radius;   // radius of the capsule (world space)
    char      name[32]; // name of the collider, for debugging purposes
};

// Wind direction and strength, used for wind forces in the simulation.
struct WindField {
    glm::vec3 direction  = glm::vec3(0.1f, 0.0f, -1.0f); // direction the wind is blowing in (world space)
    float     strength   = 1.0f; // magnitude of the wind force.
    float     turbulence = 4.0f; // randomness of the wind, higher means more gusts and variation.
};


class Cloth {
public:
    int width  = 0; // only used for grid-based cloth, ignored when loading from OBJ
    int height = 0; // only used for grid-based cloth, ignored when loading from OBJ

    std::vector<ClothParticle>  particles; // The points of the cloth. Every vertex in the OBJ becomes one of these.
    std::vector<Spring>         springs;   // The connections between particles.
    std::vector<unsigned int>   indices;   // The triangle list for rendering. Every triangle in the OBJ becomes 3 of these (vertex indices).
    std::vector<glm::vec3>      positionBuffer;  // Streamlined buffer of just the particle positions, used for rendering.

    // VAO stores the state of how vertex data is formatted, VBO is the actual vertex buffer object on the GPU (holds the vertex data), and EBO tells the GPU how to draw the vertices to form triangles.
    unsigned int VAO, VBO, EBO;

    float damping      = 0.995f;   // Velocity multiplier each frame, 0.99 means 1% velocity loss per frame. 
                                      // Required for it to eventually settle, otherwise it would oscillate forever.
    float stretchLimit = 1.1f;    // How much a spring can stretch before being force-corrected. 1.1 means 10% stretch beyond rest length is allowed.
    float inertiaScale = 1.5f;    // How strongly character's movement affects the cloth, currently unused.
    float structStiffness  = 0.5f;   // correction factor for structural springs
    float bendStiffness    = 0.005f;  // correction factor for bend springs
    glm::vec3 gravity      = glm::vec3(0.0f, -9.8f, 0.0f); // Gravity vector (world space)
    int       substeps     = 4;       // recomputed each frame, higher means a more stable simulation but more expensive to compute.

    static constexpr int SOLVER_ITERS = 3; // How many times to iterate over the springs each frame. More iterations = more stable simulation, but more expensive.

    // Returns a fully constructed Cloth object loaded from an OBJ file. 
    static Cloth loadFromOBJ(const std::string& path,
                             float pinYTolerance = 0.01f)
    {
        Cloth cloth;

        Assimp::Importer imp;
        const aiScene* scene = imp.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices);  // JoinIdenticalVertices is important to avoid duplicate vertices which break the spring system.

        if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
            printf("Cloth::loadFromOBJ — failed: %s\n", imp.GetErrorString());
            return cloth;
        }

        // Every vertex in the OBJ becomes a ClothParticle.
        for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++)
        {
            aiMesh* m    = scene->mMeshes[mi];
            int     base = (int)cloth.particles.size();

            for (unsigned int i = 0; i < m->mNumVertices; i++)
            {
                ClothParticle p;
                p.position     = glm::vec3(m->mVertices[i].x,
                                           m->mVertices[i].y,
                                           m->mVertices[i].z);
                p.prevPosition = p.position;
                p.restPosition = p.position;
                p.pinned       = false;
                cloth.particles.push_back(p);
            }

            // Every triangle becomes 3 indices pushed into cloth.indices for rendering,
            for (unsigned int i = 0; i < m->mNumFaces; i++)
            {
                aiFace& f = m->mFaces[i];
                if (f.mNumIndices != 3) continue;
                cloth.indices.push_back(base + f.mIndices[0]);
                cloth.indices.push_back(base + f.mIndices[1]);
                cloth.indices.push_back(base + f.mIndices[2]);
            }
        }

        if (cloth.particles.empty()) return cloth;

        // Finds highest Y value among the vertices, and pins all vertices within pinYTolerance of that.
        float maxY = -1e9f;
        for (auto& p : cloth.particles)
            maxY = std::max(maxY, p.restPosition.y);
        for (auto& p : cloth.particles)
            if (p.restPosition.y >= maxY - pinYTolerance)
                p.pinned = true;

        // First extract all unique edges from the triangle list using a set (no duplicates) of ordered vertex index pairs.
        std::set<std::pair<int,int>> edgeSet;
        for (size_t i = 0; i < cloth.indices.size(); i += 3)
        {
            int a = cloth.indices[i], b = cloth.indices[i+1], c = cloth.indices[i+2];
            auto addEdge = [&](int x, int y) {
                edgeSet.insert({std::min(x,y), std::max(x,y)});
            };
            addEdge(a, b); addEdge(b, c); addEdge(a, c);
        }

        // Springs are created between the computed edges (every side of each triangle gets a spring).
        // Structural springs (up down left right) and shear springs (diagonals) are created here.
        for (auto& [a, b] : edgeSet)
        {
            Spring s;
            s.a = a; s.b = b;
            s.restLength = glm::length(
                cloth.particles[a].position - cloth.particles[b].position);
            cloth.springs.push_back(s);
        }

        std::vector<std::vector<int>> adj(cloth.particles.size());
        for (auto& [a, b] : edgeSet) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }

        // Build bend springs between neighbors of neighbors, but aren't directly connected by an edge. Resists bending more so Cloth won't crease sharply at every edge.
        //             [B]  <-- 2 steps up
        //              :
        //             [ ]  <-- (neighbor)
        //              :
        // [B]. .[ ]. .(X). .[ ]. .[B]
        //              :
        //             [ ]  <-- (neighbor)
        //              :
        //             [B]  <-- 2 steps down
        //
        // (X) = Particle
        // [ ] = Immediate neighbors
        // [B] = Bending Spring connections (2 steps away)
        std::set<std::pair<int,int>> bendSet;
        for (int i = 0; i < (int)cloth.particles.size(); i++)
        for (int nb  : adj[i])
        for (int nb2 : adj[nb])
        {
            if (nb2 == i) continue;
            auto key = std::make_pair(std::min(i,nb2), std::max(i,nb2));
            if (edgeSet.count(key) == 0 && bendSet.count(key) == 0)
            {
                bendSet.insert(key);
                Spring s;
                s.a = i; s.b = nb2;
                s.isBend = true;
                s.restLength = glm::length(
                    cloth.particles[i].position - cloth.particles[nb2].position);
                cloth.springs.push_back(s);
            }
        }

        cloth.positionBuffer.resize(cloth.particles.size());
        
        cloth.setupBuffers();
        return cloth;
    }

    // NOTE: Not currently used, loadFromOBJ is the main constructor. But this could be used to create a simple rectangular cloth without needing an OBJ file.
    // Grid-based cloth constructor, creates a width*height grid of particles with the given spacing, and creates springs between adjacent particles.
    Cloth(int w, int h, float spacing, bool pinTop = true)
        : width(w), height(h)
    {
        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            ClothParticle p;
            p.position     = glm::vec3(x*spacing, -y*spacing, 0.0f);
            p.prevPosition = p.position;
            p.restPosition = p.position;
            p.pinned       = pinTop && (y == 0);
            particles.push_back(p);
        }
        positionBuffer.resize(particles.size());

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            if (x+1 < w) addSpring(idx(x,y), idx(x+1,y));
            if (y+1 < h) addSpring(idx(x,y), idx(x,y+1));
        }
        for (int y = 0; y < h-1; y++)
        for (int x = 0; x < w-1; x++)
        {
            addSpring(idx(x,y), idx(x+1,y+1));
            addSpring(idx(x+1,y), idx(x,y+1));
        }
        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            if (x+2 < w) addSpring(idx(x,y), idx(x+2,y));
            if (y+2 < h) addSpring(idx(x,y), idx(x,y+2));
        }
        for (int y = 0; y < h-1; y++)
        for (int x = 0; x < w-1; x++)
        {
            indices.push_back(idx(x,  y));   indices.push_back(idx(x+1,y));
            indices.push_back(idx(x,  y+1)); indices.push_back(idx(x+1,y));
            indices.push_back(idx(x+1,y+1)); indices.push_back(idx(x,  y+1));
        }
        setupBuffers();
    }

    // Called every frame. Uses Verlet integration, Jakobsen spring constraint solving, and Position-Based Dynamics for cloth simulation.
    // Jakobsen/PBD ensures stability by directly resolving spring constraints and collisions in position-space rather than relying on force-based acceleration.
    void update(float dt,
                const WindField& wind,
                const std::vector<CapsuleCollider>& colliders,
                const glm::mat4& modelMatrix,
                glm::vec3 characterWorldPos = glm::vec3(0.0f))
    {
        // More wind = more substeps to prevent cloth tunnelling through colliders at high speed, capped at 12 for performance.
        substeps = std::min(4 + (int)(wind.strength * 2.0f), 12);
        // NOTE: charVelocity is currently unused (character doesn't move).
        glm::vec3 charVelocity = (characterWorldPos - prevCharacterPos) / dt;
        prevCharacterPos = characterWorldPos;

        glm::mat4 invModel = glm::inverse(modelMatrix);
        float subDt = dt / substeps; // smaller subDt mean integration using smaller rectangles to find area under curve.

        for (int s = 0; s < substeps; s++)
        {
            applyForces(subDt, wind, charVelocity);              // Update particles positions based on Gravity, inertia, and wind
            satisfyConstraints();                                // Update positions with spring correction
            stretchLimitPass();                                  // Hard stretch clamp
            resolveCollisions(colliders, modelMatrix, invModel); // Push particle positions outside capsule colliders
        }

        // Damping once per frame on the implicit Verlet velocity
        // Adjust prevPosition rather than a velocity field
        for (auto& p : particles)
        {
            if (p.pinned) continue;
            glm::vec3 vel = p.position - p.prevPosition;
            p.prevPosition = p.position - vel * damping;
        }

        uploadToGPU();
    }

    void draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (unsigned int)indices.size(),
                       GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void repinByHeight(float pinYTolerance)
    {
        float maxY = -1e9f;
        for (auto& p : particles)
            maxY = std::max(maxY, p.restPosition.y);
        for (auto& p : particles)
            p.pinned = (p.restPosition.y >= maxY - pinYTolerance);
    }

private:
    glm::vec3 prevCharacterPos = glm::vec3(0.0f);

    // idx is a helper function to convert 2D grid coordinates to the corresponding index in the 1D particles array.
    int idx(int x, int y) { return y * width + x; }

    void addSpring(int a, int b)
    {
        Spring s;
        s.a = a; s.b = b;
        s.restLength = glm::length(particles[a].position - particles[b].position);
        springs.push_back(s);
    }

    // Adds velocity to each particle in the triangle based on the triangle's facing relative to the wind direction.
    void applyWindToTriangle(int i0, int i1, int i2,
                             float dt, const WindField& wind)
    {
        ClothParticle& p0 = particles[i0];
        ClothParticle& p1 = particles[i1];
        ClothParticle& p2 = particles[i2];

        glm::vec3 e1 = p1.position - p0.position;
        glm::vec3 e2 = p2.position - p0.position;
        glm::vec3 n  = glm::normalize(glm::cross(e1, e2));

        float turb = 1.0f + wind.turbulence *
            sin(glfwGetTime()*2.3f + p0.position.x*3.7f) *
            cos(glfwGetTime()*1.7f + p0.position.y*2.9f);

        // Dot product tells how directly the wind hits that face. Directly facing wind = 1.0, parallel to wind = 0.0
        glm::vec3 wf = wind.direction * wind.strength * turb;
        float     wd = glm::dot(n, glm::normalize(wf));
        // Verlet: forces add to position as accel * dt² not velocity
        glm::vec3 f = n * wd * glm::length(wf) * dt * dt / 3.0f;  // Divide the resulting force by 3.
        if (!p0.pinned) p0.position += f;
        if (!p1.pinned) p1.position += f;
        if (!p2.pinned) p2.position += f;
    }

    void applyForces(float dt, const WindField& wind, glm::vec3 charVelocity)
    {
        // NOTE: charVelocity currently not used/updated (character doesn't move).
        // Inertia is negated character velocity, so if character moves forward, the cape is pushed backward, etc. 
        // Multiplied by inertiaScale to adjust how much the character's movement affects the cape.
        glm::vec3 inertia = -charVelocity * inertiaScale;
        glm::vec3 accel   = gravity + inertia;

        for (auto& p : particles)
        {
            if (p.pinned) continue;

            glm::vec3 temp = p.position;

            // Verlet integration:
            // The three terms are:
            //   p.position                       - where we are now
            //   (p.position - p.prevPosition)    - implicit velocity (how far we moved last step)
            //   accel * dt^2                     - extra displacement from acceleration this step
            //
            // This is derived from: newPos = pos + vel*dt + accel*dt^2
            // but velocity is never stored explicitly.

            // Key advantage over explicit velocity: if a spring corrects position from
            // 1.0 to 1.1, next frame's implicit velocity becomes (1.1 - prevPos)
            // automatically. A stored velocity would still point back toward 1.0,
            // fighting the correction.
            p.position    = p.position
                        + (p.position - p.prevPosition)
                        + accel * dt * dt;
            p.prevPosition = temp;
        }

        // Wind adds directly to position after the Verlet step
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
            applyWindToTriangle(indices[i], indices[i+1], indices[i+2], dt, wind);
    }

    // Core of the simulation, for each spring, computes how stretched it is and moves both endpoints toward the rest length.
    // Jakobsen constraint projection (form of Position Based Dynamics) instead of calculating a "spring force" (F = kx), just measure the distance. 
    // If the cloth is stretching, move the particles' positions directly until the distance is correct.
    void satisfyConstraints()
    {
        // More solver iterations means stiffer springs that more aggressively correct stretching.
        for (int iter = 0; iter < SOLVER_ITERS; iter++)
        for (auto& s : springs)
        {
            ClothParticle& a = particles[s.a];
            ClothParticle& b = particles[s.b];
            glm::vec3 d  = b.position - a.position;
            float dist   = glm::length(d);
            if (dist == 0.0f) continue;
            float stretch = dist - s.restLength;

            if (s.isBend)
            {
                // Bend springs: weak correction both ways
                float corr = stretch / dist * bendStiffness;
                glm::vec3 mv = d * corr;
                if (!a.pinned) a.position += mv;
                if (!b.pinned) b.position -= mv;
            }
            else
            {
                // Structural springs: full correction when stretched,
                // NO correction when compressed so cloth can fold freely
                if (stretch > 0.0f)
                {
                    float corr   = stretch / dist * structStiffness;
                    glm::vec3 mv = d * corr;
                    if (!a.pinned) a.position += mv;
                    if (!b.pinned) b.position -= mv;
                }
            }
        }
    }

    // After the spring solver runs, this hard clamps any springs that are stretched beyond the stretchLimit, to prevent extreme stretching from things like gravity.
    void stretchLimitPass()
    {
        for (auto& s : springs)
        {
            if (s.isBend) continue; //skip stretch limit for bend springs.
            ClothParticle& a = particles[s.a];
            ClothParticle& b = particles[s.b];
            glm::vec3 d  = b.position - a.position;
            float dist   = glm::length(d);
            float maxLen = s.restLength * stretchLimit;
            if (dist > maxLen && dist > 0.0f)
            {
                float corr   = (dist - maxLen) / dist * 0.5f;
                glm::vec3 mv = d * corr;
                if (!a.pinned) a.position += mv;
                if (!b.pinned) b.position -= mv;
            }
        }
    }

    // Checks if it's inside any colliders, and if so pushes it to the surface of the collider.
    // PBD style collision response, directly moves the particle to the surface of the collider, rather than applying a force. 
    // This is more stable and prevents tunnelling at high speeds.
    void resolveCollisions(const std::vector<CapsuleCollider>& colliders,
                           const glm::mat4& modelMatrix,
                           const glm::mat4& invModel)
    {
        for (auto& p : particles)
        {
            if (p.pinned) continue;

            // Transforms particle position to world space for collision calculations.
            glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(p.position, 1.0f));

            for (auto& c : colliders)
            {
                // Find closest point on capsule line segment a-b to particle
                glm::vec3 ab   = c.b - c.a;
                float     len2 = glm::dot(ab, ab);
                float     t    = len2 > 0.0f
                                 ? glm::clamp(glm::dot(worldPos - c.a, ab) / len2,
                                              0.0f, 1.0f)
                                 : 0.0f;
                glm::vec3 closest = c.a + t * ab;

                glm::vec3 d    = worldPos - closest;
                float     dist = glm::length(d);
                
                // If the particle is within the radius of the capsule, push it out to the surface.
                if (dist < c.radius && dist > 0.0f)
                {
                    glm::vec3 worldNormal = d / dist;
                    glm::vec3 newWorldPos = closest + worldNormal * c.radius;

                    p.position = glm::vec3(invModel * glm::vec4(newWorldPos, 1.0f));  // invModel transforms back to cloth's local space

                    // Cancel inward implicit velocity by adjusting prevPosition so particles don't immediately bounce back through the collider.
                    // implicitVel = pos - prevPos
                    // new_prevPos = pos - (implicitVel - normal * inwardComponent)
                    glm::vec3 localNormal = glm::normalize(
                        glm::vec3(glm::transpose(modelMatrix) *
                                glm::vec4(worldNormal, 0.0f)));

                    glm::vec3 implicitVel = p.position - p.prevPosition;
                    // Remove implicit velocity (verlet) into the surface (only the part that points into the surface) to allow sliding and not bouncing.
                    float vd = glm::dot(implicitVel, localNormal);
                    if (vd < 0.0f) p.prevPosition += localNormal * vd;
                }
            }
        }
    }

    // Runs once at load time, creates the OpenGL buffers (GPU objects) that will hold the cloth geometry.
    void setupBuffers()
    {
        // OpenGL gives an available ID for the VAO, VBO, and EBO, which are stored for later use.
        glGenVertexArrays(1, &VAO);  // Generate 1 vertex array object and store its ID in VAO
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO); // VAO records/saves configuration of VBO and EBO state.
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     particles.size() * sizeof(glm::vec3),
                     nullptr, GL_DYNAMIC_DRAW);  // Allocates VBO eith the right size but doesn't fill it with data yet, 
                                                 // since position data is uploaded every frame in uploadToGPU().
                                                 // GL_DYNAMIC_DRAW tells OpenGL to put it in memory that's fast to write to.  
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);  // EBO is filled once with the triangle indices and never changes, 
                                                       // so GL_STATIC_DRAW is used to put it in memory optimized for static data.
        // Links this cloth to Attribute 0 'Location 0'. Even though other objects use 0, the VAO ensures the GPU only pulls from this cloth's VBO during its draw call, 
        // then the GPU can execute parallel threads for position data in the shader.
        glEnableVertexAttribArray(0); // Enables the vertex attribute at location 0 (the position attribute in the shader).
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(glm::vec3), (void*)0);  // Tells OpenGL how to interpret the VBO vertex data: attribute 0, 3 floats, no padding between them (sizeof(glm::vec3)).
        glBindVertexArray(0); // Unbinds the VAO closing the setup. Everything is saved in the VAO state.
        uploadToGPU();
    }

    // Runs every frame, updates the GPU buffer with the current particle positions so it can be rendered.
    void uploadToGPU()
    {
        // Copies positions from the CPU particle array into the intermediate buffer positionBuffer as a performance optimization.
        // This way the GPU buffer only has position data, and not all the other ClothParticle data.
        for (size_t i = 0; i < particles.size(); i++)
            positionBuffer[i] = particles[i].position;
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // Streams the position data to the GPU. SubData updates an existing buffer, which is much faster than calling glBufferData every frame.
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        positionBuffer.size() * sizeof(glm::vec3),
                        positionBuffer.data());
    }

    // Default constructor so loadFromOBJ can use it to construct a blank Cloth and fill in the data.
    Cloth() {}
};

// Debug capsule - cylinder with a hemisphere on each end
struct DebugCapsule {
    unsigned int VAO, VBO, EBO;
    int indexCount;

    DebugCapsule(int stacks = 8, int slices = 16)
    {
        std::vector<glm::vec3>    verts;
        std::vector<unsigned int> idx;

        // Top hemisphere: phi 0 (north pole) → PI/2 (equator), offset +0.5 in Y
        for (int i = 0; i <= stacks; i++)
        {
            float phi = glm::half_pi<float>() * (float)i / stacks;
            for (int j = 0; j <= slices; j++)
            {
                float theta = 2.f * glm::pi<float>() * j / slices;
                verts.push_back(glm::vec3(
                    sin(phi) * cos(theta),
                    cos(phi) + 0.5f,
                    sin(phi) * sin(theta)
                ));
            }
        }

        // Bottom hemisphere: phi PI/2 (equator) → PI (south pole), offset -0.5 in Y
        for (int i = 0; i <= stacks; i++)
        {
            float phi = glm::half_pi<float>() +
                        glm::half_pi<float>() * (float)i / stacks;
            for (int j = 0; j <= slices; j++)
            {
                float theta = 2.f * glm::pi<float>() * j / slices;
                verts.push_back(glm::vec3(
                    sin(phi) * cos(theta),
                    cos(phi) - 0.5f,
                    sin(phi) * sin(theta)
                ));
            }
        }

        // Two rings for the cylinder edges at y=+0.5 and y=-0.5
        // These exactly match where the hemispheres end so lines connect cleanly
        int cylBase = (int)verts.size();
        for (int ring = 0; ring <= 1; ring++)
        {
            float y = ring == 0 ? 0.5f : -0.5f;
            for (int j = 0; j <= slices; j++)
            {
                float theta = 2.f * glm::pi<float>() * j / slices;
                verts.push_back(glm::vec3(cos(theta), y, sin(theta)));
            }
        }

        // Hemisphere wireframe: rings + meridians for both hemispheres
        int totalRows = (stacks + 1) * 2;
        for (int i = 0; i < totalRows - 1; i++)
        for (int j = 0; j < slices; j++)
        {
            int a = i * (slices + 1) + j;
            int b = a + slices + 1;
            idx.push_back(a);     idx.push_back(b);      // meridian
            idx.push_back(a);     idx.push_back(a + 1);  // latitude ring
        }

        // Cylinder vertical lines connecting top ring to bottom ring
        for (int j = 0; j <= slices; j++)
        {
            idx.push_back(cylBase + j);
            idx.push_back(cylBase + (slices + 1) + j);
        }

        indexCount = (int)idx.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3),
                     verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int),
                     idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);
    }

    // Builds a matrix that places the unit capsule exactly over collider c.
    // X/Z scale = radius (makes the unit circle the right size).
    // Y scale   = halfSegLen + radius (halfSegLen stretches the cylinder,
    //             + radius accounts for each hemisphere being 1 unit tall).
    static glm::mat4 makeMatrix(const CapsuleCollider& c)
    {
        glm::vec3 center  = (c.a + c.b) * 0.5f;
        float     segLen  = glm::length(c.b - c.a);
        glm::vec3 dir     = segLen > 0.0001f
                            ? glm::normalize(c.b - c.a)
                            : glm::vec3(0.f, 1.f, 0.f);

        // Rotation from unit +Y to the capsule direction
        glm::vec3 up(0.f, 1.f, 0.f);
        glm::mat4 rot(1.f);
        if (glm::abs(glm::dot(dir, up)) < 0.9999f)
        {
            glm::vec3 axis  = glm::normalize(glm::cross(up, dir));
            float     angle = acos(glm::clamp(glm::dot(up, dir), -1.f, 1.f));
            rot = glm::rotate(glm::mat4(1.f), angle, axis);
        }

        glm::mat4 m = glm::translate(glm::mat4(1.f), center) * rot;
        return glm::scale(m, glm::vec3(c.radius,
                                       segLen * 0.5f + c.radius,
                                       c.radius));
    }

    void draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};