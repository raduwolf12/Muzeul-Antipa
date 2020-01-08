#include <stdlib.h> // necesare pentru citirea shaderStencilTesting-elor
#include <stdio.h>
#include <math.h> 

#include <GL/glew.h>

#define GLM_FORCE_CTOR_INIT 
#include <GLM.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "OBJ_Loader.h"
#pragma comment (lib, "glfw3dll.lib")
#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "OpenGL32.lib")

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
bool rot = false;
objl::Loader Loader;
//bool loadout = Loader.LoadFile("duck.obj");
enum ECameraMovementType
{
	UNKNOWN,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Camera
{
private:
	// Default camera values
	const float zNEAR = 0.1f;
	const float zFAR = 500.f;
	const float YAW = -90.0f;
	const float PITCH = 0.0f;
	const float FOV = 45.0f;
	glm::vec3 startPosition;

public:
	Camera(const int width, const int height, const glm::vec3& position)
	{
		startPosition = position;
		Set(width, height, position);
	}

	void Set(const int width, const int height, const glm::vec3& position)
	{
		this->isPerspective = true;
		this->yaw = YAW;
		this->pitch = PITCH;

		this->FoVy = FOV;
		this->width = width;
		this->height = height;
		this->zNear = zNEAR;
		this->zFar = zFAR;

		this->worldUp = glm::vec3(0, 1, 0);
		this->position = position;

		lastX = width / 2.0f;
		lastY = height / 2.0f;
		bFirstMouseMove = true;

		UpdateCameraVectors();
	}

	void Reset(const int width, const int height)
	{
		Set(width, height, startPosition);
	}

	void Reshape(int windowWidth, int windowHeight)
	{
		width = windowWidth;
		height = windowHeight;

		// define the viewport transformation
		glViewport(0, 0, windowWidth, windowHeight);
	}

	const glm::vec3 GetPosition() const
	{
		return position;
	}

	const glm::mat4 GetViewMatrix() const
	{
		// Returns the View Matrix
		return glm::lookAt(position, position + forward, up);
	}

	const glm::mat4 GetProjectionMatrix() const
	{
		glm::mat4 Proj = glm::mat4(1);
		if (isPerspective) {
			float aspectRatio = ((float)(width)) / height;
			Proj = glm::perspective(glm::radians(FoVy), aspectRatio, zNear, zFar);
		}
		else {
			float scaleFactor = 2000.f;
			Proj = glm::ortho<float>(
				-width / scaleFactor, width / scaleFactor,
				-height / scaleFactor, height / scaleFactor, -zFar, zFar);
		}
		return Proj;
	}

	void ProcessKeyboard(ECameraMovementType direction, float deltaTime)
	{
		float velocity = (float)(cameraSpeedFactor * deltaTime);
		switch (direction) {
		case ECameraMovementType::FORWARD:
			position += forward * velocity;
			break;
		case ECameraMovementType::BACKWARD:
			position -= forward * velocity;
			break;
		case ECameraMovementType::LEFT:
			position -= right * velocity;
			break;
		case ECameraMovementType::RIGHT:
			position += right * velocity;
			break;
		case ECameraMovementType::UP:
			position += up * velocity;
			break;
		case ECameraMovementType::DOWN:
			position -= up * velocity;
			break;
		}
	}

	void MouseControl(float xPos, float yPos)
	{
		if (bFirstMouseMove) {
			lastX = xPos;
			lastY = yPos;
			bFirstMouseMove = false;
		}

		float xChange = xPos - lastX;
		float yChange = lastY - yPos;
		lastX = xPos;
		lastY = yPos;

		if (fabs(xChange) <= 1e-6 && fabs(yChange) <= 1e-6) {
			return;
		}
		xChange *= mouseSensitivity;
		yChange *= mouseSensitivity;

		ProcessMouseMovement(xChange, yChange);
	}

	void ProcessMouseScroll(float yOffset)
	{
		if (FoVy >= 1.0f && FoVy <= 90.0f) {
			FoVy -= yOffset;
		}
		if (FoVy <= 1.0f)
			FoVy = 1.0f;
		if (FoVy >= 90.0f)
			FoVy = 90.0f;
	}

private:
	void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
	{
		yaw += xOffset;
		pitch += yOffset;

		//std::cout << "yaw = " << yaw << std::endl;
		//std::cout << "pitch = " << pitch << std::endl;

		// Avem grijã sã nu ne dãm peste cap
		if (constrainPitch) {
			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;
		}

		// Se modificã vectorii camerei pe baza unghiurilor Euler
		UpdateCameraVectors();
	}

	void UpdateCameraVectors()
	{
		// Calculate the new forward vector
		this->forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward.y = sin(glm::radians(pitch));
		this->forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward = glm::normalize(this->forward);
		// Also re-calculate the Right and Up vector
		right = glm::normalize(glm::cross(forward, worldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		up = glm::normalize(glm::cross(right, forward));
	}

protected:
	const float cameraSpeedFactor = 2.5f;
	const float mouseSensitivity = 0.1f;

	// Perspective properties
	float zNear;
	float zFar;
	float FoVy;
	int width;
	int height;
	bool isPerspective;

	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 worldUp;

	// Euler Angles
	float yaw;
	float pitch;

	bool bFirstMouseMove = true;
	float lastX = 0.f, lastY = 0.f;
};

class Shader
{
public:
	// constructor generates the shaderStencilTesting on the fly
	// ------------------------------------------------------------------------
	Shader(const char* vertexPath, const char* fragmentPath)
	{
		Init(vertexPath, fragmentPath);
	}

	~Shader()
	{
		glDeleteProgram(ID);
	}

	// activate the shaderStencilTesting
	// ------------------------------------------------------------------------
	void Use() const
	{
		glUseProgram(ID);
	}

	unsigned int GetID() const { return ID; }

	// MVP
	unsigned int loc_model_matrix;
	unsigned int loc_view_matrix;
	unsigned int loc_projection_matrix;

	// utility uniform functions
	void SetInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetFloat(const std::string& name, float value) const
	{
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetVec3(const std::string& name, const glm::vec3& value) const
	{
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}
	void SetVec3(const std::string& name, float x, float y, float z) const
	{
		glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	}
	void SetMat4(const std::string& name, const glm::mat4& mat) const
	{
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

private:
	void Init(const char* vertexPath, const char* fragmentPath)
	{
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::ifstream vShaderFile;
		std::ifstream fShaderFile;
		// ensure ifstream objects can throw exceptions:
		vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try {
			// open files
			vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			std::stringstream vShaderStream, fShaderStream;
			// read file's buffer contents into streams
			vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();
			// close file handlers
			vShaderFile.close();
			fShaderFile.close();
			// convert stream into string
			vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
		}
		catch (std::ifstream::failure e) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}
		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();

		// 2. compile shaders
		unsigned int vertex, fragment;
		// vertex shaderStencilTesting
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);
		CheckCompileErrors(vertex, "VERTEX");
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		CheckCompileErrors(fragment, "FRAGMENT");
		// shaderStencilTesting Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		glLinkProgram(ID);
		CheckCompileErrors(ID, "PROGRAM");

		// 3. delete the shaders as they're linked into our program now and no longer necessery
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}

	// utility function for checking shaderStencilTesting compilation/linking errors.
	// ------------------------------------------------------------------------
	void CheckCompileErrors(unsigned int shaderStencilTesting, std::string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shaderStencilTesting, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
		else {
			glGetProgramiv(shaderStencilTesting, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}
private:
	unsigned int ID;
};

Camera* pCamera = nullptr;

unsigned int CreateTexture(const std::string& strTexturePath)
{
	unsigned int textureId = -1;

	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
	unsigned char* data = stbi_load(strTexturePath.c_str(), &width, &height, &nrChannels, 0);
	if (data) {
		GLenum format;
		if (nrChannels == 1)
			format = GL_RED;
		else if (nrChannels == 3)
			format = GL_RGB;
		else if (nrChannels == 4)
			format = GL_RGBA;

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		std::cout << "Failed to load texture: " << strTexturePath << std::endl;
	}
	stbi_image_free(data);

	return textureId;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

//textures
void renderScene(const Shader& shader);
void renderWall(const Shader& shader);
void renderDino(const Shader& shader);
void renderPeacock(const Shader& shader);
void renderTurkeyVulture(const Shader& shader);
void renderTree(const Shader& shader);
void renderOwl(const Shader& shader);
void renderGlassWindows(const Shader& shader);
void renderGlassPlatform(const Shader& shader);
void renderLeafTree(const Shader& shader);
void renderLeafTree(const Shader& shader);
void renderIce(const Shader& shader);
//objects
void renderCube();
void renderFloor();
void renderDino();
void renderDino2();
void renderTrexTop();
void renderTrexBottom();
void renderPeacock();
void renderTurkeyVulture();
void renderTree();
void renderOwl();
void renderGlassWindows();
void renderGlassPlatform();
void renderLeafTree();
void renderHat();
void renderHat0();
void renderHat1();
void renderHat2();
void renderHat3();
void renderHat4();
void renderHat5();
void renderHat6();
//floor
void renderRoomF();
//room
void renderRoomF1();
void renderRoomF2();
void renderRoomF3();
void renderRoomF4();
void renderRoomF5();


// timing
double deltaTime = 0.0f;    // time between current frame and last frame
double lastFrame = 0.0f;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_L && action == GLFW_PRESS)
	{
		rot = true;
	}
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		rot = false;
	}

}

GLuint VAO, VBO, EBO;
void CreateVBO()
{
	float vertices[] = {
-1, -1, 1,0, 0, 1,0.375, 0,
1, -1, 1,0, 0, 1,0.625, 0,
1, 1, 1,0, 0, 1,0.625, 0.25,
-1, 1, 1,0, 0, 1,0.375, 0.25,
-1, 1, 1,0, 1, 0,0.375, 0.25,
1, 1, 1,0, 1, 0,0.625, 0.25,
1, 1, -1,0, 1, 0,0.625, 0.5,
-1, 1, -1,0, 1, 0,0.375, 0.5,
-1, 1, -1,0, 0, -1,0.375, 0.5,
1, 1, -1,0, 0, -1,0.625, 0.5,
1, -1, -1,0, 0, -1,0.625, 0.75,
-1, -1, -1,0, 0, -1,0.375, 0.75,
-1, -1, -1,0, -1, 0,0.375, 0.75,
1, -1, -1,0, -1, 0,0.625, 0.75,
1, -1, 1,0, -1, 0,0.625, 1,
-1, -1, 1,0, -1, 0,0.375, 1,
1, -1, 1,1, 0, 0,0.625, 0,
1, -1, -1,1, 0, 0,0.875, 0,
1, 1, -1,1, 0, 0,0.875, 0.25,
1, 1, 1,1, 0, 0,0.625, 0.25,
-1, -1, -1,-1, 0, 0,0.125, 0,
-1, -1, 1,-1, 0, 0,0.375, 0,
-1, 1, 1,-1, 0, 0,0.375, 0.25,
-1, 1, -1,-1, 0, 0,0.125, 0.25
	};

	unsigned int indices[] = {
0, 1, 3,
1, 2, 3,
4, 5, 7,
5, 6, 7,
8, 9, 11,
9, 10, 11,
12, 13, 15,
13, 14, 15,
16, 17, 19,
17, 18, 19,
 20, 21, 23,
 21, 22, 23
	};


	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
}




int main(int argc, char** argv)
{
	std::string strFullExeFileName = argv[0];
	std::string strExePath;
	const size_t last_slash_idx = strFullExeFileName.rfind('\\');
	if (std::string::npos != last_slash_idx) {
		strExePath = strFullExeFileName.substr(0, last_slash_idx);
	}

	// glfw: initialize and configure
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Exploatarea muzeului Antipa", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);
	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewInit();
	
	

	// Create camera
	pCamera = new Camera(SCR_WIDTH, SCR_HEIGHT, glm::vec3(0.0, 1.0, 3.0));

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders
	// -------------------------
	Shader shadowMappingShader("ShadowMapping.vs", "ShadowMapping.fs");
	Shader shadowMappingDepthShader("ShadowMappingDepth.vs", "ShadowMappingDepth.fs");

	// load textures
	// -------------
	unsigned int floorTexture1 = CreateTexture(strExePath + "\\Wall4.jpg");
	unsigned int floorTexture = CreateTexture(strExePath + "\\Floor4.jpg");
	unsigned int wallTexture = CreateTexture(strExePath + "\\Wall.jpg");
	unsigned int platformTexture = CreateTexture(strExePath + "\\black.jpg");
	unsigned int peacockTexture = CreateTexture(strExePath + "\\Peacock.jpg");
	unsigned int treeTexture = CreateTexture(strExePath + "\\wood.jpg");
	unsigned int owlTexture = CreateTexture(strExePath + "\\owl.jpg");
	unsigned int leafTreeTexture = CreateTexture(strExePath + "\\leaftree.jpg");
	unsigned int iceTexture = CreateTexture(strExePath + "\\ice.png");


	// configure depth map FBO
	// -----------------------
	const unsigned int SHADOW_WIDTH = 4098, SHADOW_HEIGHT = 4098;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create depth texture
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// shader configuration
	// --------------------
	shadowMappingShader.Use();
	shadowMappingShader.SetInt("diffuseTexture", 0);
	shadowMappingShader.SetInt("shadowMap", 1);

	// lighting info
	// -------------
	glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

	glEnable(GL_CULL_FACE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;



		if (rot)
		{
			lightPos.x = 2.0 * sin(currentFrame);
			lightPos.z = 2.0 * cos(currentFrame);

		}


		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	

		// 1. render depth of scene to texture (from light's perspective)
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		float near_plane = 1.0f, far_plane = 7.5f;
		lightProjection = glm::ortho(10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;

		// render scene from light's point of view
		shadowMappingDepthShader.Use();
		shadowMappingDepthShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderScene(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, wallTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderWall(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture1);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderDino(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, owlTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderOwl(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, peacockTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderPeacock(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, peacockTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderTurkeyVulture(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, leafTreeTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderLeafTree(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, treeTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderTree(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture1);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderGlassWindows(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);



		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, platformTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderGlassPlatform(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, iceTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderIce(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// reset viewport
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 2. render scene as normal using the generated depth/shadow map 
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shadowMappingShader.Use();
		glm::mat4 projection = pCamera->GetProjectionMatrix();
		glm::mat4 view = pCamera->GetViewMatrix();
		shadowMappingShader.SetMat4("projection", projection);
		shadowMappingShader.SetMat4("view", view);
		// set light uniforms
		shadowMappingShader.SetVec3("viewPos", pCamera->GetPosition());
		shadowMappingShader.SetVec3("lightPos", lightPos);
		shadowMappingShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderScene(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, wallTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderWall(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderDino(shadowMappingShader);


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, platformTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderGlassPlatform(shadowMappingShader);

	



		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, peacockTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderPeacock(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, iceTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderIce(shadowMappingShader);


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, treeTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderTree(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, owlTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderOwl(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, leafTreeTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderLeafTree(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, peacockTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderTurkeyVulture(shadowMappingShader);


		//obiect transparent
		glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		renderGlassWindows(shadowMappingShader);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	delete pCamera;

	glfwTerminate();
	return 0;
}

// renders the 3D scene
// --------------------
void renderScene(const Shader& shader)
{
	// floor
	glm::mat4 model;
	shader.SetMat4("model", model);
	renderFloor();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF();

	
}

void renderGlassWindows(const Shader& shader)
{
	//window 
	glm::mat4 model;
	model = glm::mat4();
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderGlassWindows();

}
void renderGlassPlatform(const Shader& shader)
{
	//platform
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderGlassPlatform();


	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-9.5f, -0.5f, -9.0f));
	model = glm::scale(model, glm::vec3(0.7f));
	shader.SetMat4("model", model);
	renderGlassPlatform();
}


void renderPeacock(const Shader& shader)
{
	//cube
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(4.0f, 3.2f, -17.7f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderPeacock();

}
void renderTree(const Shader& shader)
{
	//cube
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(5.0f, -1.7f, -20.0f));
	model = glm::scale(model, glm::vec3(0.101f));
	shader.SetMat4("model", model);
	renderTree();

}

void renderLeafTree(const Shader& shader)
{
	//cube
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-5.0f, -1.5f, -20.0f));
	model = glm::scale(model, glm::vec3(0.101f));
	shader.SetMat4("model", model);
	renderTree();

}

void renderOwl (const Shader& shader)
{
	//render owl
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(6.5f, 3.2f, -17.7f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderOwl();

}
void renderTurkeyVulture(const Shader& shader)
{
	//cube
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-3.0f, -0.4f, -16.0f));
	model = glm::scale(model, glm::vec3(1.2f));
	shader.SetMat4("model", model);
	renderTurkeyVulture();
}
void renderIce(const Shader& shader)
{
	//cube
	float angle =120.f;

	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.0f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));

	shader.SetMat4("model", model);
	renderHat();
	//glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.0f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));

	shader.SetMat4("model", model);
	renderHat0(); //glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.0f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model,angle  , glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat1();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.00f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat2();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.00f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat3();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.00f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat4();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.00f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat5();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(17.5f, 1.00f, 14.5f));
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	//model=glm::rotate()
	shader.SetMat4("model", model);
	renderHat6();

	//model = glm::mat4();
	//model = glm::translate(model, glm::vec3(6.0f, 0.0f, 0.0f));
	//model = glm::scale(model, glm::vec3(0.05f));
	////model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	////model=glm::rotate()
	//shader.SetMat4("model", model);
	//renderHat7();
}
void renderDino(const Shader& shader)
{
	//dino
	glm::mat4 model;
	


	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -0.2f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderDino();


	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-15.0f, -0.2f, 10.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderDino2();

	/*model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderTrexBottom();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderTrexTop();*/
}
void renderWall(const Shader& shader)
{
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF1();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF2();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF3();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF4();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	renderRoomF5();
}
unsigned int planeVAO = 0;
void renderFloor()
{
	unsigned int planeVBO;

	if (planeVAO == 0) {
		// set up vertex data (and buffer(s)) and configure vertex attributes
		float planeVertices[] = {
			// positions            // normals         // texcoords
			25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
			-25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
			-25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

			25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
			-25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  0.0f, 25.0f,
			25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
		};
		// plane VAO
		glGenVertexArrays(1, &planeVAO);
		glGenBuffers(1, &planeVBO);
		glBindVertexArray(planeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindVertexArray(0);
	}

	glBindVertexArray(planeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}


// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			// bottom face
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			// top face
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
/*unsigned int animalVAO = 0;
unsigned int animalVBO = 0;
unsigned int animalEBO = 0;*/
float vertices0[82000];
unsigned int indices0[72000];


GLuint iceVAO00, iceVBO00, iceEBO00;

void renderHat()
{
	// initialize (if necessary)
	if (iceVAO00 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices0[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices0[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO00);
		glGenBuffers(1, &iceVBO00);
		glGenBuffers(1, &iceEBO00);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO00);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices0), vertices0, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO00);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices0), &indices0, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO00);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO00);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO00);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO00);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
float vertices01[82000];
unsigned int indices01[72000];

GLuint iceVAO01, iceVBO01, iceEBO01;
void renderHat0()
{
	// initialize (if necessary)
	if (iceVAO01 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[1];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices01[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices01[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO01);
		glGenBuffers(1, &iceVBO01);
		glGenBuffers(1, &iceEBO01);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO01);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices01), vertices01, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO01);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices01), &indices01, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO01);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO01);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO01);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO01);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
float vertices02[82000];
unsigned int indices02[72000];
GLuint iceVAO02, iceVBO02, iceEBO02;
void renderHat1()
{
	// initialize (if necessary)
	if (iceVAO02 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[2];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices02[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices02[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO02);
		glGenBuffers(1, &iceVBO02);
		glGenBuffers(1, &iceEBO02);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO02);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices02), vertices02, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO02);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices02), &indices02, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO02);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO02);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO02);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO02);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices03[82000];
unsigned int indices03[72000];
GLuint iceVAO03, iceVBO03, iceEBO03;

void renderHat2()
{
	// initialize (if necessary)
	if (iceVAO03 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[3];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices03[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices03[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO03);
		glGenBuffers(1, &iceVBO03);
		glGenBuffers(1, &iceEBO03);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO03);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices03), vertices03, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO03);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices03), &indices03, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO03);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO03);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO03);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO03);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}



float vertices04[82000];
unsigned int indices04[72000];
GLuint iceVAO04, iceVBO04, iceEBO04;

void renderHat3()
{
	// initialize (if necessary)
	if (iceVAO04 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[4];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices04[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices04[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO04);
		glGenBuffers(1, &iceVBO04);
		glGenBuffers(1, &iceEBO04);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO04);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices04), vertices04, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO04);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices04), &indices04, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO04);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO04);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO04);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO04);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}



float vertices05[82000];
unsigned int indices05[72000];
GLuint iceVAO05, iceVBO05, iceEBO05;

void renderHat4()
{
	// initialize (if necessary)
	if (iceVAO05 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[5];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices05[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices05[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO05);
		glGenBuffers(1, &iceVBO05);
		glGenBuffers(1, &iceEBO05);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO05);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices05), vertices05, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO05);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices05), &indices05, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO05);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO05);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO05);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO05);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices06[82000];
unsigned int indices06[72000];
GLuint iceVAO06, iceVBO06, iceEBO06;

void renderHat5()
{
	// initialize (if necessary)
	if (iceVAO06 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[6];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices06[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices06[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO06);
		glGenBuffers(1, &iceVBO06);
		glGenBuffers(1, &iceEBO06);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO06);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices06), vertices06, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO06);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices06), &indices06, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO06);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO06);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO06);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO06);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices07[82000];
unsigned int indices07[72000];
GLuint iceVAO07, iceVBO07, iceEBO07;

void renderHat6()
{
	// initialize (if necessary)
	if (iceVAO07 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("pinguin1.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[7];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices07[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices07[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &iceVAO07);
		glGenBuffers(1, &iceVBO07);
		glGenBuffers(1, &iceEBO07);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, iceVBO07);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices07), vertices07, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO07);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices07), &indices07, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(iceVAO07);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(iceVAO07);
	glBindBuffer(GL_ARRAY_BUFFER, iceVBO07);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iceEBO07);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
float vertices1[82000];
unsigned int indices1[72000];
float vertices[82000];


GLuint peacockVAO, peacockVBO, peacockEBO;

void renderPeacock()
{
	// initialize (if necessary)
	if (peacockVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("peeacock.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices1[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices1[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &peacockVAO);
		glGenBuffers(1, &peacockVBO);
		glGenBuffers(1, &peacockEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, peacockVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices1), vertices1, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, peacockEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices1), &indices1, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(peacockVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(peacockVAO);
	glBindBuffer(GL_ARRAY_BUFFER, peacockVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, peacockEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices2[82000];
unsigned int indices2[72000];
GLuint treeVAO, treeVBO, treeEBO;

void renderTree()
{
	// initialize (if necessary)
	if (treeVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("tree.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices2[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices2[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &treeVAO);
		glGenBuffers(1, &treeVBO);
		glGenBuffers(1, &treeEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, treeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, treeEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2), &indices2, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(treeVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(treeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, treeVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, treeEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}


float vertices12[82000];
unsigned int indices12[72000];
GLuint ltreeVAO, ltreeVBO, ltreeEBO;

void renderLeafTree()
{
	// initialize (if necessary)
	if (ltreeVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("leaftree.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices12[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices12[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &ltreeVAO);
		glGenBuffers(1, &ltreeVBO);
		glGenBuffers(1, &ltreeEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, ltreeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices12), vertices12, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ltreeEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices12), &indices12, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(ltreeVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(ltreeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, ltreeVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ltreeEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}




float vertices3[82000];
unsigned int indices3[72000];

GLuint turkeyVultureVAO, turkeyVultureVBO, turkeyVultureEBO;

void renderTurkeyVulture()
{
	// initialize (if necessary)
	if (turkeyVultureVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("turkeyvulture.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices3[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices3[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &turkeyVultureVAO);
		glGenBuffers(1, &turkeyVultureVBO);
		glGenBuffers(1, &turkeyVultureEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, turkeyVultureVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices3), vertices3, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, turkeyVultureEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices3), &indices3, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(turkeyVultureVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(turkeyVultureVAO);
	glBindBuffer(GL_ARRAY_BUFFER, turkeyVultureVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, turkeyVultureEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}


unsigned int indices[72000];
objl::Vertex ver[82000];

GLuint animalVAO, animalVBO, animalEBO;
void renderDino()
{
	// initialize (if necessary)
	if (animalVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("dino.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		objl::Vertex v;
		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{
			v.Position.X = (float)curMesh.Vertices[j].Position.X;
			v.Position.Y = (float)curMesh.Vertices[j].Position.Y;
			v.Position.Z = (float)curMesh.Vertices[j].Position.Z;
			v.Normal.X = (float)curMesh.Vertices[j].Normal.X;
			v.Normal.Y = (float)curMesh.Vertices[j].Normal.Y;
			v.Normal.Z = (float)curMesh.Vertices[j].Normal.Z;
			v.TextureCoordinate.X = (float)curMesh.Vertices[j].TextureCoordinate.X;
			v.TextureCoordinate.Y = (float)curMesh.Vertices[j].TextureCoordinate.Y;

			
			ver[j] = v;
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &animalVAO);
		glGenBuffers(1, &animalVBO);
		glGenBuffers(1, &animalEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, animalVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(ver), ver, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, animalEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(animalVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(animalVAO);
	glBindBuffer(GL_ARRAY_BUFFER, animalVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, animalEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}


unsigned int indicesD[72000];
objl::Vertex verD[82000];

GLuint dino2VAO, dino2VBO, dino2EBO;
void renderDino2()
{
	// initialize (if necessary)
	if (dino2VAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("dino2.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();
		objl::Vertex v;
		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{
			v.Position.X = (float)curMesh.Vertices[j].Position.X;
			v.Position.Y = (float)curMesh.Vertices[j].Position.Y;
			v.Position.Z = (float)curMesh.Vertices[j].Position.Z;
			v.Normal.X = (float)curMesh.Vertices[j].Normal.X;
			v.Normal.Y = (float)curMesh.Vertices[j].Normal.Y;
			v.Normal.Z = (float)curMesh.Vertices[j].Normal.Z;
			v.TextureCoordinate.X = (float)curMesh.Vertices[j].TextureCoordinate.X;
			v.TextureCoordinate.Y = (float)curMesh.Vertices[j].TextureCoordinate.Y;


			verD[j] = v;
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicesD[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &dino2VAO);
		glGenBuffers(1, &dino2VBO);
		glGenBuffers(1, &dino2EBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, dino2VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(verD), verD, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dino2EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicesD), &indicesD, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(dino2VAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(dino2VAO);
	glBindBuffer(GL_ARRAY_BUFFER, dino2VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dino2EBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}



objl::Vertex ver1[82000];
GLuint platformVAO, platformVBO, platformEBO;
void renderGlassPlatform()
{
	if (platformVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("platformWindow.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		objl::Vertex v;
		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{
			v.Position.X = (float)curMesh.Vertices[j].Position.X;
			v.Position.Y = (float)curMesh.Vertices[j].Position.Y;
			v.Position.Z = (float)curMesh.Vertices[j].Position.Z;
			v.Normal.X = (float)curMesh.Vertices[j].Normal.X;
			v.Normal.Y = (float)curMesh.Vertices[j].Normal.Y;
			v.Normal.Z = (float)curMesh.Vertices[j].Normal.Z;
			v.TextureCoordinate.X = (float)curMesh.Vertices[j].TextureCoordinate.X;
			v.TextureCoordinate.Y = (float)curMesh.Vertices[j].TextureCoordinate.Y;
			ver1[j] = v;
		}
		/*for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}*/

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices1[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &platformVAO);
		glGenBuffers(1, &platformVBO);
		glGenBuffers(1, &platformEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, platformVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(ver1), ver1, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices1), &indices1, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(platformVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(platformVAO);
	glBindBuffer(GL_ARRAY_BUFFER, platformVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

GLuint glassVAO, glassVBO, glassEBO;
void renderGlassWindows()
{
	if (glassVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("glassWindow.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		objl::Vertex v;
		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{
			v.Position.X = (float)curMesh.Vertices[j].Position.X;
			v.Position.Y = (float)curMesh.Vertices[j].Position.Y;
			v.Position.Z = (float)curMesh.Vertices[j].Position.Z;
			v.Normal.X = (float)curMesh.Vertices[j].Normal.X;
			v.Normal.Y = (float)curMesh.Vertices[j].Normal.Y;
			v.Normal.Z = (float)curMesh.Vertices[j].Normal.Z;
			v.TextureCoordinate.X = (float)curMesh.Vertices[j].TextureCoordinate.X;
			v.TextureCoordinate.Y = (float)curMesh.Vertices[j].TextureCoordinate.Y;
			ver1[j] = v;
		}
		/*for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}*/

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices1[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &glassVAO);
		glGenBuffers(1, &glassVBO);
		glGenBuffers(1, &glassEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, glassVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(ver1), ver1, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glassEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices1), &indices1, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(glassVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(glassVAO);
	glBindBuffer(GL_ARRAY_BUFFER, glassVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glassEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices8[82000];
unsigned int indices8[72000];


GLuint owlVAO, owlVBO, owlEBO;

void renderOwl()
{
	// initialize (if necessary)
	if (owlVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("owl.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices8[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices8[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &owlVAO);
		glGenBuffers(1, &owlVBO);
		glGenBuffers(1, &owlEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, owlVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices8), vertices8, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, owlEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices8), &indices8, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(owlVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(owlVAO);
	glBindBuffer(GL_ARRAY_BUFFER, owlVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, owlEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float vertices5[82000];
unsigned int indices5[72000];
GLuint trexTopVAO, trexTopVBO, trexTopEBO;
void renderTrexTop()
{
	if (trexTopVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("Trex.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[1];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices5[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices5[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &trexTopVAO);
		glGenBuffers(1, &trexTopVBO);
		glGenBuffers(1, &trexTopEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, trexTopVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices5), vertices5, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexTopEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices5), &indices5, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(trexTopVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(trexTopVAO);
	glBindBuffer(GL_ARRAY_BUFFER, trexTopVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexTopEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
float vertices6[82000];
unsigned int indices6[72000];
GLuint trexBottomVAO, trexBottomVBO, trexBottomEBO;
void renderTrexBottom()
{
	if (trexBottomVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("Trex.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices6[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices6[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &trexBottomVAO);
		glGenBuffers(1, &trexBottomVBO);
		glGenBuffers(1, &trexBottomEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, trexBottomVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices6), vertices6, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexBottomEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices6), &indices6, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(trexBottomVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(trexBottomVAO);
	glBindBuffer(GL_ARRAY_BUFFER, trexBottomVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexBottomEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
float vertices7[82000];
unsigned int indices7[72000];
GLuint floorVAO, floorVBO, floorEBO;
void renderRoomF()
{
	// initialize (if necessary)
	if (floorVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices7[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO);
		glGenBuffers(1, &floorVBO);
		glGenBuffers(1, &floorEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices7), &indices7, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}

GLuint floorVAO1, floorVBO1, floorEBO1;
void renderRoomF1()
{
	// initialize (if necessary)
	if (floorVAO1 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		for (int i = 0; i < Loader.LoadedMeshes.size(); i++)
		{
			objl::Mesh curMesh = Loader.LoadedMeshes[1];
			int size = curMesh.Vertices.size();

			for (int j = 0; j < curMesh.Vertices.size(); j++)
			{

				verticess.push_back((float)curMesh.Vertices[j].Position.X);
				verticess.push_back((float)curMesh.Vertices[j].Position.Y);
				verticess.push_back((float)curMesh.Vertices[j].Position.Z);
				verticess.push_back((float)curMesh.Vertices[j].Normal.X);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
			}
			for (int j = 0; j < verticess.size(); j++)
			{
				vertices[j] = verticess.at(j);
			}

			for (int j = 0; j < curMesh.Indices.size(); j++)
			{

				indicess.push_back((float)curMesh.Indices[j]);

			}
			for (int j = 0; j < curMesh.Indices.size(); j++)
			{
				indices[j] = indicess.at(j);
			}
		}
		glGenVertexArrays(1, &floorVAO1);
		glGenBuffers(1, &floorVBO1);
		glGenBuffers(1, &floorEBO1);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO1);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO1);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO1);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO1);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO1);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO2, floorVBO2, floorEBO2;

void renderRoomF2()
{
	// initialize (if necessary)
	if (floorVAO2 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		for (int i = 0; i < Loader.LoadedMeshes.size(); i++)
		{
			objl::Mesh curMesh = Loader.LoadedMeshes[2];
			int size = curMesh.Vertices.size();

			for (int j = 0; j < curMesh.Vertices.size(); j++)
			{

				verticess.push_back((float)curMesh.Vertices[j].Position.X);
				verticess.push_back((float)curMesh.Vertices[j].Position.Y);
				verticess.push_back((float)curMesh.Vertices[j].Position.Z);
				verticess.push_back((float)curMesh.Vertices[j].Normal.X);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
			}
			for (int j = 0; j < verticess.size(); j++)
			{
				vertices[j] = verticess.at(j);
			}

			for (int j = 0; j < curMesh.Indices.size(); j++)
			{

				indicess.push_back((float)curMesh.Indices[j]);

			}
			for (int j = 0; j < curMesh.Indices.size(); j++)
			{
				indices[j] = indicess.at(j);
			}
		}
		glGenVertexArrays(1, &floorVAO2);
		glGenBuffers(1, &floorVBO2);
		glGenBuffers(1, &floorEBO2);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO2);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO2);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO2);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO2);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO2);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO2);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO3, floorVBO3, floorEBO3;

void renderRoomF3()
{
	// initialize (if necessary)
	if (floorVAO3 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[3];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO3);
		glGenBuffers(1, &floorVBO3);
		glGenBuffers(1, &floorEBO3);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO3);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO3);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO3);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO3);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO3);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO4, floorVBO4, floorEBO4;

void renderRoomF4()
{
	// initialize (if necessary)
	if (floorVAO4 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[4];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO4);
		glGenBuffers(1, &floorVBO4);
		glGenBuffers(1, &floorEBO4);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO4);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO4);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO4);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO4);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO4);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO4);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO5, floorVBO5, floorEBO5;

void renderRoomF5()
{
	// initialize (if necessary)
	if (floorVAO5 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("room.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[5];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO5);
		glGenBuffers(1, &floorVBO5);
		glGenBuffers(1, &floorEBO5);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO5);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO5);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO5);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO5);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO5);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO5);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		pCamera->ProcessKeyboard(FORWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		pCamera->ProcessKeyboard(BACKWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		pCamera->ProcessKeyboard(LEFT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		pCamera->ProcessKeyboard(RIGHT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
		pCamera->ProcessKeyboard(UP, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
		pCamera->ProcessKeyboard(DOWN, (float)deltaTime);

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		pCamera->Reset(width, height);

	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	pCamera->Reshape(width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	pCamera->MouseControl((float)xpos, (float)ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yOffset)
{
	pCamera->ProcessMouseScroll((float)yOffset);
}