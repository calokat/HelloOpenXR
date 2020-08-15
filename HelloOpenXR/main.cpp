#include <iostream>
#include <fstream>
#include <string>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "GLFW/glfw3native.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Shader.h"
#include <Windows.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include "openxr/openxr.h"
#include "openxr/openxr_reflection.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_reflection.h"
#include "openxr//xr_linear.h"
#include <vector>
#include <algorithm>
#include <list>
#include <map>
#include <chrono>
#include <thread>

GLint WIDTH = 800, HEIGHT = 600;
// camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame
std::list<std::vector<XrSwapchainImageOpenGLKHR>> m_swapchainImageBuffers;

XrEventDataBuffer m_eventDataBuffer;
XrInstance m_instance;
XrSession m_session;
const XrViewConfigurationType m_viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
std::vector<XrView> m_views;
bool m_sessionRunning = false;
struct Swapchain {
	XrSwapchain handle;
	int32_t width;
	int32_t height;
};
std::vector<Swapchain> m_swapchains;
std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
XrSessionState m_sessionState{ XR_SESSION_STATE_UNKNOWN };
XrSpace m_appSpace;
std::map<uint32_t, uint32_t> m_colorToDepthMap;

GLuint m_swapchainFramebuffer;
GLuint m_program;
unsigned int rbo;
GLuint texture;

GLFWwindow* m_window;

glm::vec3 cubePositions[] = {
glm::vec3(0.0f,  0.0f,  0.0f),
glm::vec3(2.0f,  5.0f, -15.0f),
glm::vec3(-1.5f, -2.2f, -2.5f),
glm::vec3(-3.8f, -2.0f, -12.3f),
glm::vec3(2.4f, -0.4f, -3.5f),
glm::vec3(-1.7f,  3.0f, -7.5f),
glm::vec3(1.3f, -2.0f, -2.5f),
glm::vec3(1.5f,  2.0f, -2.5f),
glm::vec3(1.5f,  0.2f, -1.5f),
glm::vec3(-1.3f,  1.0f, -1.5f)
};


void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	float cameraSpeed = 2.5f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}


float lastX = 400, lastY = 300;
float pitch, yaw = -90.f;
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{

	float xoffset = (float)xpos - lastX;
	float yoffset = lastY - (float)ypos;
	lastX = (float)xpos;
	lastY = (float)ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}
float Zoom = 45;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Zoom -= (float)yoffset;
	if (Zoom < 1.0f)
		Zoom = 1.0f;
	if (Zoom > 45.0f)
		Zoom = 45.0f;
}

void window_resize_callback(GLFWwindow* window, int newWidth, int newHeight)
{
	glfwSetWindowSize(window, newWidth, newHeight);
	WIDTH = newWidth;
	HEIGHT = newHeight;
	int newScreenWidth, newScreenHeight;
	glfwGetFramebufferSize(window, &newScreenWidth, &newScreenHeight);
	glViewport(0, 0, newScreenWidth, newScreenHeight);

}

glm::mat4 calculate_lookAt_matrix(glm::vec3 position, glm::vec3 target, glm::vec3 worldUp)
{
	// 1. Position = known
	// 2. Calculate cameraDirection
	glm::vec3 zaxis = glm::normalize(position - target);
	// 3. Get positive right axis vector
	glm::vec3 xaxis = glm::normalize(glm::cross(glm::normalize(worldUp), zaxis));
	// 4. Calculate camera up vector
	glm::vec3 yaxis = glm::cross(zaxis, xaxis);

	// Create translation and rotation matrix
	// In glm we access elements as mat[col][row] due to column-major layout
	glm::mat4 translation = glm::mat4(1.0f); // Identity matrix by default
	translation[3][0] = -position.x; // Third column, first row
	translation[3][1] = -position.y;
	translation[3][2] = -position.z;
	glm::mat4 rotation = glm::mat4(1.0f);
	rotation[0][0] = xaxis.x; // First column, first row
	rotation[1][0] = xaxis.y;
	rotation[2][0] = xaxis.z;
	rotation[0][1] = yaxis.x; // First column, second row
	rotation[1][1] = yaxis.y;
	rotation[2][1] = yaxis.z;
	rotation[0][2] = zaxis.x; // First column, third row
	rotation[1][2] = zaxis.y;
	rotation[2][2] = zaxis.z;

	// Return lookAt matrix as combination of translation and rotation matrix
	return rotation * translation; // Remember to read from right to left (first translation then rotation)
}

const XrEventDataBaseHeader* TryReadNextEvent() 
{
	XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
	*baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
	const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
	if (xr == XR_SUCCESS) {
		if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
			const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
		}
		return baseHeader;
	}
	if (xr == XR_EVENT_UNAVAILABLE) {
		return nullptr;
	}
}

void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop,
	bool* requestRestart) {
	const XrSessionState oldState = m_sessionState;
	m_sessionState = stateChangedEvent.state;

	if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
		std::cout << "XrEventDataSessionStateChanged for unknown session" << std::endl;
		return;
	}

	switch (m_sessionState) {
	case XR_SESSION_STATE_READY: {
		XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
		sessionBeginInfo.primaryViewConfigurationType = m_viewConfigType;
		m_sessionRunning = true;
		xrBeginSession(m_session, &sessionBeginInfo);
		break;
	}
	case XR_SESSION_STATE_STOPPING: {
		m_sessionRunning = false;
		xrEndSession(m_session);
		break;
	}
	case XR_SESSION_STATE_EXITING: {
		*exitRenderLoop = true;
		// Do not attempt to restart because user closed this session.
		*requestRestart = false;
		break;
	}
	case XR_SESSION_STATE_LOSS_PENDING: {
		*exitRenderLoop = true;
		// Poll for a new instance.
		*requestRestart = true;
		break;
	}
	default:
		*exitRenderLoop = true;
		*requestRestart = false;
		break;
	}
}


void PollEvent(bool* exitRenderLoop, bool* requestRestart)
{
	// Process all pending messages.
	while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
		switch (event->type) {
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
			const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
			return;
		}
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
			auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
			HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
			break;
		}
		case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		default: {
			std::cout << "Ignoring event type " << event->type << std::endl;
			break;
		}
		}
	}
}

uint32_t GetDepthTexture(uint32_t colorTexture) {
	// If a depth-stencil view has already been created for this back-buffer, use it.
	auto depthBufferIt = m_colorToDepthMap.find(colorTexture);
	if (depthBufferIt != m_colorToDepthMap.end()) {
		return depthBufferIt->second;
	}

	// This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

	GLint width;
	GLint height;
	glBindTexture(GL_TEXTURE_2D, colorTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	glBindTexture(GL_TEXTURE_2D, 0);

	uint32_t depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	
	m_colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture));

	return depthTexture;
}

uint32_t SetDepth(uint32_t colorTexture)
{
	GLint width;
	GLint height;
	//glBindTexture(GL_TEXTURE_2D, colorTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	//glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	//glBindRenderbuffer(GL_RENDERBUFFER, 0);

	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

	return rbo;
}

void RenderScene(XrCompositionLayerProjectionView view)
{
	XrMatrix4x4f projection;

	XrMatrix4x4f_CreateProjectionFov(&projection, GraphicsAPI::GRAPHICS_OPENGL, view.fov, .5f, 100);

	XrMatrix4x4f toView;
	XrPosef pose = view.pose;
	XrVector3f scale = { 1.f, 1.f, 1.f };
	XrVector3f posOffset = { 0, 0, 3 };
	XrVector3f_Add(&pose.position, &pose.position, &posOffset);
	XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);

	XrMatrix4x4f viewMat;
	XrMatrix4x4f_InvertRigidBody(&viewMat, &toView);

	XrMatrix4x4f vp;
	XrMatrix4x4f_Multiply(&vp, &projection, &viewMat);

	glm::mat4 model(1.0f);

	GLint modelLoc = glGetUniformLocation(m_program, "model");
	GLint viewLoc = glGetUniformLocation(m_program, "view");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMat.m);
	GLint projLoc = glGetUniformLocation(m_program, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection.m);


	for (unsigned int j = 0; j < 10; j++)
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[j]);
		float angle = 20.0f * j;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}


}

void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);
	
	const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;

	glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
		static_cast<GLint>(layerView.subImage.imageRect.offset.y),
		static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
		static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));



	//glFrontFace(GL_CW);
	//glCullFace(GL_BACK);
	//glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	GLuint depthTexture = GetDepthTexture(colorTexture);
	
	glBindTexture(GL_TEXTURE_2D, texture);
	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

	//uint32_t renderBuffer = SetDepth(colorTexture);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
	
	GLenum err = glGetError();
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);


	glClearColor(1, 1, 1, 1);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	RenderScene(layerView);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	static int everyOther = 0;
	if ((everyOther++ & 1) != 0) {
		glfwSwapBuffers(m_window);
	}

}

bool RenderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
	XrCompositionLayerProjection& layer) {
	XrResult res;

	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	uint32_t viewCapacityInput = (uint32_t)m_views.size();
	uint32_t viewCountOutput;

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = m_viewConfigType;
	viewLocateInfo.displayTime = predictedDisplayTime;
	viewLocateInfo.space = m_appSpace;

	res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());
	if (XR_UNQUALIFIED_SUCCESS(res)) {
		bool r1 = (viewCountOutput == viewCapacityInput);
		bool r3 =(viewCountOutput == m_swapchains.size());

		projectionLayerViews.resize(viewCountOutput);
		// Render view to the appropriate part of the swapchain image.
		for (uint32_t i = 0; i < viewCountOutput; i++) {
			// Each view has a separate swapchain which is acquired, rendered to, and released.
			const Swapchain viewSwapchain = m_swapchains[i];

			XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };

			uint32_t swapchainImageIndex;
			XrResult acquireSwapchainResult = xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex);

			XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
			waitInfo.timeout = XR_INFINITE_DURATION;
			XrResult waitSwapchainResult =  xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo);

			projectionLayerViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
			projectionLayerViews[i].pose = m_views[i].pose;
			projectionLayerViews[i].fov = m_views[i].fov;
			projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
			projectionLayerViews[i].subImage.imageRect.offset = { 0, 0 };
			projectionLayerViews[i].subImage.imageRect.extent = { viewSwapchain.width, viewSwapchain.height };

			const XrSwapchainImageBaseHeader* const swapchainImage =
				m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
			RenderView(projectionLayerViews[i], swapchainImage);

			XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			XrResult releaseSwapchainResult = xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo);
		}

		layer.space = m_appSpace;
		layer.viewCount = (uint32_t)projectionLayerViews.size();
		layer.views = projectionLayerViews.data();
		return true;
	}
	return false;
}

void RenderFrame()
{
	XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	XrFrameState frameState{ XR_TYPE_FRAME_STATE };
	XrResult waitResult = xrWaitFrame(m_session, &frameWaitInfo, &frameState);

	XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
	XrResult beginResult = xrBeginFrame(m_session, &frameBeginInfo);

	std::vector<XrCompositionLayerBaseHeader*> layers;
	XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
	if (frameState.shouldRender == XR_TRUE) {
		if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews, layer)) {
			layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
		}
	}

	XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
	frameEndInfo.displayTime = frameState.predictedDisplayTime;
	frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	frameEndInfo.layerCount = (uint32_t)layers.size();
	frameEndInfo.layers = layers.data();
	XrResult endResult = xrEndFrame(m_session, &frameEndInfo);

}

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "OpenXR", nullptr, nullptr);
	int screenWidth, screenHeight;
	glfwGetFramebufferSize(m_window, &screenWidth, &screenHeight);
	glfwSetCursorPosCallback(m_window, mouse_callback);
	glfwSetScrollCallback(m_window, scroll_callback);
	glfwSetWindowSizeCallback(m_window, window_resize_callback);
	// Check if the window creation was successful by checking if the window object is a null pointer or not
	if (m_window == nullptr)
	{
		// If the window returns a null pointer, meaning the window creation was not successful
		// we print out the messsage and terminate the glfw using glfwTerminate()
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();

		// Since the application was not able to create a window we exit the program returning EXIT_FAILURE
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(m_window);
	glewExperimental = GL_TRUE;

	if (GLEW_OK != glewInit())
	{
		// If the initalization is not successful, print out the message and exit the program with return value EXIT_FAILURE
		std::cout << "Failed to initialize GLEW" << std::endl;

		return EXIT_FAILURE;
	}


	//texture stuff
	int width, height, nChannels;
	unsigned char* data = stbi_load("../resources/texture.png", &width, &height, &nChannels, 0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load image" << std::endl;
	}
	stbi_image_free(data);

	glViewport(0, 0, screenWidth, screenHeight);

	// XR STUFF

	HWND hWnd = glfwGetWin32Window(m_window);
	HDC hDC = GetDC(hWnd);
	HGLRC hGlrc = glfwGetWGLContext(m_window);


	uint32_t nApiLayerProps;
	xrEnumerateApiLayerProperties(0, &nApiLayerProps, nullptr);
	std::vector<XrApiLayerProperties> layerProps(nApiLayerProps);
	xrEnumerateApiLayerProperties(nApiLayerProps, &nApiLayerProps, layerProps.data());
	std::cout << "Number of Api Layer Properties: " << nApiLayerProps << std::endl;
	for (int i = 0; i < nApiLayerProps; ++i)
	{
		std::cout << layerProps[i].layerName << std::endl;
	}
	uint32_t nInstanceExtensionProps;
	xrEnumerateInstanceExtensionProperties(nullptr, 0, &nInstanceExtensionProps, nullptr);
	std::vector<XrExtensionProperties> extensionProps(nInstanceExtensionProps, {XR_TYPE_EXTENSION_PROPERTIES});
	xrEnumerateInstanceExtensionProperties(nullptr, nInstanceExtensionProps, &nInstanceExtensionProps, extensionProps.data());

	XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.createFlags = 0;
	createInfo.enabledApiLayerCount = 0;
	createInfo.enabledApiLayerNames = nullptr;
	createInfo.enabledExtensionCount = 1;
	std::string glExtension = "XR_KHR_opengl_enable";
	const char* cGlStr = glExtension.c_str();
	//createInfo.enabledExtensionNames = extensionNames.data();
	createInfo.enabledExtensionNames = &cGlStr;
	strcpy_s(createInfo.applicationInfo.applicationName, "HelloOpenXR");
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	XrResult createInstanceResult = xrCreateInstance(&createInfo, &m_instance);

	// initialize system
	XrSystemId systemId;
	XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrResult getSystemResult = xrGetSystem(m_instance, &systemInfo, &systemId);
	
	// get instance properties
	XrInstanceProperties instanceProps = {XR_TYPE_INSTANCE_PROPERTIES};
	XrResult getInstanceResult = xrGetInstanceProperties(m_instance, &instanceProps);
	// get system properties
	XrSystemProperties systemProps = {XR_TYPE_SYSTEM_PROPERTIES};
	XrResult getSystemPropsResult = xrGetSystemProperties(m_instance, systemId, &systemProps);


	uint32_t nEnvBlendModes;
	xrEnumerateEnvironmentBlendModes(m_instance, systemId, m_viewConfigType, 0, &nEnvBlendModes, nullptr);
	std::vector<XrEnvironmentBlendMode> blendModes(nEnvBlendModes);
	xrEnumerateEnvironmentBlendModes(m_instance, systemId, m_viewConfigType, nEnvBlendModes, &nEnvBlendModes, blendModes.data());

	uint32_t nConfigViews;
	xrEnumerateViewConfigurations(m_instance, systemId, 0, &nConfigViews, nullptr);
	std::vector<XrViewConfigurationType> configViews(nConfigViews);
	xrEnumerateViewConfigurations(m_instance, systemId, nConfigViews, &nConfigViews, configViews.data());

	XrViewConfigurationProperties viewConfigProps = {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
	xrGetViewConfigurationProperties(m_instance, systemId, m_viewConfigType, &viewConfigProps);

	uint32_t nViewConfigViews;
	xrEnumerateViewConfigurationViews(m_instance, systemId, m_viewConfigType, 0, &nViewConfigViews, nullptr);
	std::vector<XrViewConfigurationView> viewConfigViews(nViewConfigViews, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
	XrResult enumerateViewConfigViewsResult = xrEnumerateViewConfigurationViews(m_instance, systemId, m_viewConfigType, nViewConfigViews, &nViewConfigViews, viewConfigViews.data());

	m_views.resize(nViewConfigViews, { XR_TYPE_VIEW });

	PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
	XrResult getPFN = xrGetInstanceProcAddr(m_instance, "xrGetOpenGLGraphicsRequirementsKHR",
		reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLGraphicsRequirementsKHR));

	XrGraphicsRequirementsOpenGLKHR reqs{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
	XrResult getReqsResult = pfnGetOpenGLGraphicsRequirementsKHR(m_instance, systemId, &reqs);

	XrGraphicsBindingOpenGLWin32KHR oglBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
	oglBinding.hDC = hDC;
	oglBinding.hGLRC = hGlrc;
	oglBinding.next = nullptr;
	// create session
	XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.createFlags = 0;
	sessionInfo.next = reinterpret_cast<const XrBaseInStructure*>(&oglBinding);
	sessionInfo.systemId = systemId;
	XrResult createSessionResult = xrCreateSession(m_instance, &sessionInfo, &m_session);
	if (!XR_SUCCEEDED(createSessionResult))
	{
		return EXIT_FAILURE;
	}

	uint32_t nRefSpaces;
	xrEnumerateReferenceSpaces(m_session, 0, &nRefSpaces, nullptr);
	std::vector<XrReferenceSpaceType> refSpaces(nRefSpaces);
	xrEnumerateReferenceSpaces(m_session, nRefSpaces, &nRefSpaces, refSpaces.data());
	std::vector<XrSpace> spaces;

	for (XrReferenceSpaceType refSpaceType : refSpaces)
	{
		XrPosef pose = {};
		pose.orientation.w = 1;
		const XrReferenceSpaceCreateInfo refSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr, refSpaceType, pose};
		XrSpace potentialSpace;
		XrResult createSpaceResult = xrCreateReferenceSpace(m_session, &refSpaceCreateInfo, &potentialSpace);
		if (XR_SUCCEEDED(createSpaceResult))
		{
			spaces.push_back(potentialSpace);
		}
		else
		{
			std::cout << "Uh oh" << std::endl;
		}
	}
	XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	XrPosef pose = {};
	pose.orientation.w = 1;
	referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	referenceSpaceCreateInfo.poseInReferenceSpace = pose;
	XrResult createAppSpace = xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace);


	uint32_t nFormats;
	xrEnumerateSwapchainFormats(m_session, 0, &nFormats, nullptr);
	std::vector<int64_t> formats(nFormats);
	XrResult enumSwapChainResult = xrEnumerateSwapchainFormats(m_session, nFormats, &nFormats, formats.data());
	

	for (uint32_t i = 0; i < nViewConfigViews; ++i)
	{
		const XrViewConfigurationView& vp = viewConfigViews[i];
		XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
		for (int64_t format : formats)
		{
			if (format == GL_RGBA8 || format == GL_RGBA8_SNORM)
			{
				swapchainCreateInfo.format = format;
				break;
			}
		}
		swapchainCreateInfo.arraySize = 1;
		swapchainCreateInfo.width = vp.recommendedImageRectWidth;
		swapchainCreateInfo.height = vp.recommendedImageRectHeight;
		swapchainCreateInfo.mipCount = 1;
		swapchainCreateInfo.faceCount = 1;
		swapchainCreateInfo.sampleCount = vp.recommendedSwapchainSampleCount;
		swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		Swapchain swapchain;
		swapchain.width = swapchainCreateInfo.width;
		swapchain.height = swapchainCreateInfo.height;
		XrResult createSwapChainResult = xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle);
		m_swapchains.push_back(swapchain);
		uint32_t imageCount;
		XrResult enumImages = xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr);

		std::vector<XrSwapchainImageBaseHeader*> swapchainImages;
		std::vector<XrSwapchainImageOpenGLKHR> swapchainImageBuffer(imageCount);
		for (XrSwapchainImageOpenGLKHR& image : swapchainImageBuffer) {
			image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
			swapchainImages.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
		}
		m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

		XrResult enumSwapchainImageResult = xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]);

		m_swapchainImages.insert(std::make_pair(swapchain.handle, swapchainImages));
	}

	GLuint VAO, VBO, IBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &IBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	GLint vShader, fShader;
	m_program = glCreateProgram();
	
	glGenFramebuffers(1, &m_swapchainFramebuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);

	glGenRenderbuffers(1, &rbo);


	Shader vertexShader = Shader("../shaders/vertex.glsl", GL_VERTEX_SHADER);
	vertexShader.Compile();

	Shader fragShader = Shader("../shaders/fragment.glsl", GL_FRAGMENT_SHADER);
	fragShader.Compile();
	glAttachShader(m_program, vertexShader.GetId());
	glAttachShader(m_program, fragShader.GetId());
	glLinkProgram(m_program);
	glUseProgram(m_program);

	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};


	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);



	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, (GLvoid*)0);


	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, (GLvoid*)(sizeof(GLfloat) * 3));

	bool requestRestart = false;

	while (!glfwWindowShouldClose(m_window))
	{
		bool exitRenderLoop = false;

		PollEvent(&exitRenderLoop, &requestRestart);
		if (exitRenderLoop)
		{
			break;
		}
		glfwPollEvents();
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		processInput(m_window);
		if (m_sessionRunning)
		{
			RenderFrame();
		}
		else 
		{
			// Throttle loop since xrWaitFrame won't be called.
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}

	}
	for (Swapchain s : m_swapchains)
	{
		xrDestroySwapchain(s.handle);
	}

	for (XrSpace space : spaces)
	{
		xrDestroySpace(space);
	}

	xrDestroySpace(m_appSpace);

	xrDestroySession(m_session);
	
	xrDestroyInstance(m_instance);


	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	
	glDeleteShader(vertexShader.GetId());
	glDeleteShader(fragShader.GetId());
	glDeleteProgram(m_program);



	glfwTerminate();

	return EXIT_SUCCESS;
}
