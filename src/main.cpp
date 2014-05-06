#include "common.hpp"
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <stdlib.h>
#include <stdio.h>

#include "gfx/program.hpp"
#include "gfx/geometry.hpp"
#include "gfx/texture.hpp"
#include "gfx/image.hpp"

#include "scene/camera.hpp"

#include "ui/imgui.hpp"
#include "ui/font.hpp"
#include "ui/renderer.hpp"

#include "fs/path.hpp"
#include "fs/folderwatcher.hpp"

#include "voxel/grid.hpp"
#include "voxel/mesher.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

struct MeshPhysicsGeometry: btTriangleIndexVertexArray, noncopyable
{
    vector<vec3> vertices;
    vector<unsigned int> indices;
    
    MeshPhysicsGeometry(const vector<voxel::MeshVertex>& vb, const vector<unsigned int>& ib)
    {
        vertices.reserve(vb.size());
        for (auto& v: vb) vertices.push_back(v.position);
        
        indices = ib;
        
        btIndexedMesh mesh;
        mesh.m_numTriangles = ib.size() / 3;
        mesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(indices.data());
        mesh.m_triangleIndexStride = 3 * sizeof(unsigned int);
        mesh.m_numVertices = vb.size();
        mesh.m_vertexBase = reinterpret_cast<const unsigned char*>(vertices.data());
        mesh.m_vertexStride = sizeof(vec3);

        addIndexedMesh(mesh);
    }
};

struct Mesh
{
    unique_ptr<Geometry> geometry;
    unsigned int geometryIndices;
    
    unique_ptr<MeshPhysicsGeometry> physicsGeometry;
    unique_ptr<btCollisionShape> physicsShape;
    
    static Mesh create(const vector<voxel::MeshVertex>& vb, const vector<unsigned int>& ib)
    {
        if (ib.empty())
            return Mesh();
        
        shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(voxel::MeshVertex), vb.size(), Buffer::Usage_Static);
        shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);
    
        gvb->upload(0, vb.data(), vb.size() * sizeof(voxel::MeshVertex));
        gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
        
        vector<Geometry::Element> layout =
        {
            Geometry::Element(offsetof(voxel::MeshVertex, position), Geometry::Format_Float3),
            Geometry::Element(offsetof(voxel::MeshVertex, normal), Geometry::Format_Float3),
        };
        
        auto physicsGeometry = make_unique<MeshPhysicsGeometry>(vb, ib);
        
        unique_ptr<btCollisionShape> physicsShape(new btBvhTriangleMeshShape(physicsGeometry.get(), true));
        
        return Mesh { make_unique<Geometry>(layout, gvb, gib), static_cast<unsigned int>(ib.size()), move(physicsGeometry), move(physicsShape) };
    }
};

struct PhysicsBody
{
    btDynamicsWorld* world;
    
    btDefaultMotionState motionState;
    btRigidBody* rigidBody;
    
    PhysicsBody(btDynamicsWorld* world, btCollisionShape* shape, float mass)
    : world(world)
    , rigidBody(nullptr)
    {
        btVector3 localInertia;
        
        if (mass > 0)
            shape->calculateLocalInertia(mass, localInertia);
        else
            localInertia = btVector3(0, 0, 0);
        
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, &motionState, shape, localInertia);
        
        rigidBody = new btRigidBody(rbInfo);
        
        world->addRigidBody(rigidBody);
    }
    
    ~PhysicsBody()
    {
        world->removeRigidBody(rigidBody);
        delete rigidBody;
    }
};

struct MeshInstance
{
    shared_ptr<Mesh> mesh;
    unique_ptr<PhysicsBody> body;
};

MeshInstance generateMesh(btDynamicsWorld* world, const voxel::Grid& grid)
{
    unique_ptr<voxel::Mesher> mesher = voxel::createMesherMarchingCubes();
    
    voxel::Box box = grid.read(voxel::Region(glm::i32vec3(-32, -32, 0), glm::i32vec3(32, 32, 32)));
    
    auto p = mesher->generate(box, vec3(-32, -32, 0), 1, voxel::MeshOptions {});
    
    shared_ptr<Mesh> mesh = make_shared<Mesh>(Mesh::create(p.first, p.second));
    unique_ptr<PhysicsBody> body = make_unique<PhysicsBody>(world, mesh->physicsShape.get(), 0.f);
    
    return { mesh, move(body) };
}

void generateWorld(voxel::Grid& grid)
{
    voxel::Box box(64, 64, 32);
    
    for (int z = 0; z < box.getDepth(); ++z)
        for (int y = 0; y < box.getHeight(); ++y)
            for (int x = 0; x < box.getWidth(); ++x)
            {
                voxel::Cell& c = box(x, y, z);
                
                float hill = ((x - 32) / 8.f) * ((x - 32) / 8.f) + ((y - 32) / 8.f) * ((y - 32) / 8.f);
                
                c.occupancy = (z < 5) ? 255 : (z > 10) ? 0 : (1.f - glm::clamp(sqrtf(hill), 0.f, 1.f)) * 255;
                c.material = 1;
            }
    
    grid.write(voxel::Region(glm::i32vec3(-32, -32, 0), glm::i32vec3(32, 32, 32)), box);
}

void brushWorld(voxel::Grid& grid, const vec3& position, float radius)
{
    glm::i32vec3 min = glm::i32vec3(glm::floor(position - radius));
    glm::i32vec3 max = glm::i32vec3(glm::ceil(position + radius));
    voxel::Region region(min, max);
    
    voxel::Box box = grid.read(region);
    
    for (int z = 0; z < box.getDepth(); ++z)
        for (int y = 0; y < box.getHeight(); ++y)
            for (int x = 0; x < box.getWidth(); ++x)
            {
                voxel::Cell& c = box(x, y, z);
                
                float r = glm::distance(position, vec3(x, y, z) + vec3(min));
                
                c.occupancy = std::max(c.occupancy, static_cast<unsigned char>((1 - glm::clamp(r / radius, 0.f, 1.f)) * 200.f));
            }
    
    grid.write(region, box);
}

pair<unique_ptr<Geometry>, unsigned int> generateSphere(float radius)
{
    vector<voxel::MeshVertex> vb;
    vector<unsigned int> ib;
    
    int U = 10, V = 20;
    
    for (int ui = 0; ui < U; ++ui)
        for (int vi = 0; vi < V; ++vi)
        {
            float u = float(ui) / (U - 1) * glm::pi<float>();
            float v = float(vi) / (V - 1) * 2 * glm::pi<float>();
        
            vec3 p = vec3(glm::cos(v) * glm::sin(u), glm::sin(v) * glm::sin(u), glm::cos(u));
            
            vb.push_back({p * radius, p});
        }
    
    for (int ui = 0; ui < U; ++ui)
    {
        int un = (ui + 1) % U;
        
        for (int vi = 0; vi < V; ++vi)
        {
            int vn = (vi + 1) % V;
            
            ib.push_back(ui * V + vi);
            ib.push_back(un * V + vi);
            ib.push_back(un * V + vn);
            
            ib.push_back(ui * V + vi);
            ib.push_back(un * V + vn);
            ib.push_back(ui * V + vn);
        }
    }
    
    shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(voxel::MeshVertex), vb.size(), Buffer::Usage_Static);
    shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);

    gvb->upload(0, vb.data(), vb.size() * sizeof(voxel::MeshVertex));
    gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
    
    vector<Geometry::Element> layout =
    {
        Geometry::Element(offsetof(voxel::MeshVertex, position), Geometry::Format_Float3),
        Geometry::Element(offsetof(voxel::MeshVertex, normal), Geometry::Format_Float3),
    };
    
    return make_pair(make_unique<Geometry>(layout, gvb, gib), ib.size());
}

bool wireframe = false;
Camera camera;
vec3 cameraAngles;
vec3 brushPosition;
float brushRadius = 1.f;

bool keyDown[GLFW_KEY_LAST];
bool mouseDown[GLFW_MOUSE_BUTTON_LAST];
double mouseDeltaX, mouseDeltaY;
double mouseLastX, mouseLastY;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
        keyDown[key] = true;
    
    if (action == GLFW_RELEASE)
        keyDown[key] = false;
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        wireframe = !wireframe;
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
        mouseDown[button] = true;
    
    if (action == GLFW_RELEASE)
        mouseDown[button] = false;
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    mouseDeltaX += (xpos - mouseLastX);
    mouseDeltaY += (ypos - mouseLastY);
    
    mouseLastX = xpos;
    mouseLastY = ypos;
}

static void scrollCallback(GLFWwindow* window, double xoff, double yoff)
{
    brushRadius = glm::clamp(brushRadius + float(yoff), 1.f, 20.f);
}

static void errorCallback(int error, const char* description)
{
    fputs(description, stderr);
}

void dumpTexture(Texture* tex, const string& path)
{
    Image image(Texture::Type_2D, tex->getFormat(), tex->getWidth(), tex->getHeight(), 1, 1);
    
    tex->download(0, 0, 0, image.getData(0, 0, 0), Texture::getImageSize(tex->getFormat(), tex->getWidth(), tex->getHeight()));
    
    image.saveToPNG(path);
}

string getBasePath(const string& exepath, const string& marker)
{
    string path = Path::full(exepath);
    
    for (int i = 0; i < 5; ++i)
    {
        string::size_type slash = path.find_last_of('/');
        if (slash == string::npos)
            break;
        
        path.erase(slash);
        
        if (Path::exists(path + "/" + marker))
            return path;
    }
    
    throw runtime_error("Failed to find base path");
}

int main(int argc, const char** argv)
{
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    glfwWindowHint(GLFW_VISIBLE, false);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(1280, 720, "sandvox", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    
    glfwShowWindow(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    string basePath = getBasePath(argv[0], "data");
    
    FolderWatcher fw(basePath);
    ProgramManager pm(basePath + "/src/shaders", &fw);
    TextureManager tm(basePath + "/data", &fw);
    
    ui::FontLibrary fonts(1024, 1024);
    fonts.addFont("sans", basePath + "/data/Roboto-Regular.ttf");
    
    auto brushGeom = generateSphere(1.f);
    
    camera.setView(vec3(0.5f, 20.f, 22.f), quat(cameraAngles = vec3(0.f, 0.83f, 4.683f)));
    
    glfwGetCursorPos(window, &mouseLastX, &mouseLastY);
    
    clock_t frameStamp = clock();
    
	btDefaultCollisionConfiguration collisionConfiguration;
	btCollisionDispatcher collisionDispatcher(&collisionConfiguration);
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld dynamicsWorld(&collisionDispatcher, &broadphase, &solver, &collisionConfiguration);
	
	dynamicsWorld.setGravity(btVector3(0,-10,0));

    voxel::Grid grid;
    generateWorld(grid);
    
    MeshInstance chunk = generateMesh(&dynamicsWorld, grid);
    
    ui::Renderer uir(fonts, pm.get("ui-vs", "ui-fs"));
    
    while (!glfwWindowShouldClose(window))
    {
        clock_t frameStampNew = clock();
        double frameTime = (frameStampNew - frameStamp) / double(CLOCKS_PER_SEC);
        frameStamp = frameStampNew;
        
        dynamicsWorld.stepSimulation(frameTime);
        
        glfwPollEvents();
        
        {
            vec3 ccposition = camera.getPosition();
            quat ccorientation = camera.getOrientation();
            
            vec3 offset;
            
            if (keyDown[GLFW_KEY_W] || keyDown[GLFW_KEY_UP])
                offset.x += 1;
            
            if (keyDown[GLFW_KEY_S] || keyDown[GLFW_KEY_DOWN])
                offset.x -= 1;
            
            if (keyDown[GLFW_KEY_A] || keyDown[GLFW_KEY_LEFT])
                offset.y += 1;
            
            if (keyDown[GLFW_KEY_D] || keyDown[GLFW_KEY_RIGHT])
                offset.y -= 1;
            
            float moveAmount = 2000 * frameTime;
            
            ccposition += (ccorientation * offset) * moveAmount;
            
            if (mouseDown[GLFW_MOUSE_BUTTON_RIGHT])
            {
                float rotateAmount = glm::radians(400.f) * frameTime;
            
                cameraAngles += vec3(0.f, rotateAmount * mouseDeltaY, -rotateAmount * mouseDeltaX);
            }
                
            ccorientation = quat(cameraAngles);
            
            camera.setView(ccposition, ccorientation);
        }
        
        mouseDeltaX = 0;
        mouseDeltaY = 0;
    
        fw.processChanges();
        
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        
        int framebufferWidth, framebufferHeight;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        
        float density = windowHeight == 0 ? 1 : float(framebufferHeight) / windowHeight;
        
        camera.setProjection(float(windowWidth) / windowHeight, glm::radians(60.f), 0.1f, 1000.f);
        
        mat4 viewproj = camera.getViewProjectionMatrix();
        
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            vec4 clip0(xpos / windowWidth * 2 - 1, 1 - ypos / windowHeight * 2, 0, 1);
            vec4 clip1(xpos / windowWidth * 2 - 1, 1 - ypos / windowHeight * 2, 1, 1);
            
            vec4 pw0 = glm::inverse(viewproj) * clip0;
            vec4 pw1 = glm::inverse(viewproj) * clip1;
            
            vec3 w0(pw0 / pw0.w);
            vec3 w1(pw1 / pw1.w);
            
            btDynamicsWorld::ClosestRayResultCallback callback(btVector3(w0.x, w0.y, w0.z), btVector3(w1.x, w1.y, w1.z));
            
            dynamicsWorld.rayTest(callback.m_rayFromWorld, callback.m_rayToWorld, callback);
            
            if (callback.hasHit())
            {
                brushPosition = vec3(callback.m_hitPointWorld.getX(), callback.m_hitPointWorld.getY(), callback.m_hitPointWorld.getZ());
                
                if (mouseDown[GLFW_MOUSE_BUTTON_LEFT])
                {
                    brushWorld(grid, brushPosition, brushRadius);
                    chunk = generateMesh(&dynamicsWorld, grid);
                }
            }
        }
        
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(168.f / 255.f, 197.f / 255.f, 236.f / 255.f, 1.0f);
        glClearDepth(1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        
        if (Program* prog = pm.get("terrain-vs", "terrain-fs"))
        {
            prog->bind();
            
            tm.get("grass.png").get()->bind(0);
            tm.get("ground.png").get()->bind(1);
            
            glUniform1i(prog->getHandle("AlbedoTop"), 0);
            glUniform1i(prog->getHandle("AlbedoSide"), 1);
            glUniformMatrix4fv(prog->getHandle("ViewProjection"), 1, false, glm::value_ptr(viewproj));
            
            if (chunk.mesh->geometry)
                chunk.mesh->geometry->draw(Geometry::Primitive_Triangles, 0, chunk.mesh->geometryIndices);
        }
        
        if (Program* prog = pm.get("brush-vs", "brush-fs"))
        {
            prog->bind();
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            mat4 world = glm::translate(glm::mat4(), brushPosition) * glm::scale(glm::mat4(), vec3(brushRadius));
            
            glUniformMatrix4fv(prog->getHandle("World"), 1, false, glm::value_ptr(world));
            glUniformMatrix4fv(prog->getHandle("ViewProjection"), 1, false, glm::value_ptr(viewproj));
            
            if (brushGeom.first)
                brushGeom.first->draw(Geometry::Primitive_Triangles, 0, brushGeom.second);
        }
        
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        
        uir.begin(windowWidth, windowHeight, density);
        
        const char* text1 = "Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich";
        const char* text2 = "Ταχίστη αλώπηξ βαφής ψημένη γη, δρασκελίζει υπέρ νωθρού κυνός";
        const char* text3 = "דג סקרן שט בים מאוכזב ולפתע מצא חברה";
        const char* text4 = "田居に出で 菜摘むわれをぞ 君召すと 求食り追ひゆく 山城の 打酔へる子ら 藻葉干せよ え舟繋けぬ";
        const char* text5 = "Разъяренный чтец эгоистично бьёт пятью жердями шустрого фехтовальщика.";
 
        uir.rect(vec2(10, 200) - 1.f, vec2(100, 100) + 2.f, 5, vec4(1, 1, 1, 0.5));
        uir.rect(vec2(10, 200), vec2(100, 100), 5, vec4(0.2, 0.2, 0.2, 0.5));
        
        uir.rect(vec2(210, 200) - 1.f, vec2(100, 100) + 2.f, 10, vec4(1, 1, 1, 0.5));
        uir.rect(vec2(210, 200), vec2(100, 100), 10, vec4(0.2, 0.2, 0.2, 0.5));
        
        uir.rect(vec2(10, 400) - 1.f, vec2(100, 100) + 2.f, 20, vec4(1, 1, 1, 0.5));
        uir.rect(vec2(10, 400), vec2(100, 100), 20, vec4(0.2, 0.2, 0.2, 0.5));
        
        uir.rect(vec2(210, 400) - 1.f, vec2(100, 100) + 2.f, 40, vec4(1, 1, 1, 0.5));
        uir.rect(vec2(210, 400), vec2(100, 100), 40, vec4(0.2, 0.2, 0.2, 0.5));
        
        uir.rect(vec2(10, 600) - 1.f, vec2(100, 100) + 2.f, 50, vec4(1, 1, 1, 0.5));
        uir.rect(vec2(10, 600), vec2(100, 100), 50, vec4(0.2, 0.2, 0.2, 0.5));
        
        uir.text(vec2(10, 30), "sans", text1, 18, vec4(1));
        uir.text(vec2(10, 100), "sans", text2, 24, vec4(1));
        uir.text(vec2(10, 200), "sans", text3, 28, vec4(1));
        uir.text(vec2(10, 350), "sans", text4, 36, vec4(1));
        uir.text(vec2(10, 500), "sans", text5, 25, vec4(0, 0, 0, 1));
        
        uir.end();
        
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}