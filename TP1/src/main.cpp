#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Debug.hpp"
#include "Shaders.hpp"

#define VERSION 20250901

Window main_window; // ventana principal: muestra el modelo en 3d sobre el que se pinta
Window aux_window; // ventana auxiliar que muestra la textura

void drawMain(); // dibuja el modelo "normalmente" para la ventana principal
void drawBack(); // dibuja el modelo con un shader alternativo para convertir coords de la ventana a coords de textura
void drawAux(); // dibuja la textura en la ventana auxiliar
void drawImGui(Window &window); // settings sub-window

float radius = 5; // radio del "pincel" con el que pintamos en la textura
glm::vec4 color = { 0.f, 0.f, 0.f, 1.f }; // color actual con el que se pinta en la textura

Texture texture; // textura (compartida por ambas ventanas)
Image image; // imagen (para la textura, Image est� en RAM, Texture la env�a a GPU)

Model model_chookity; // el objeto a pintar, para renderizar en la ventan principal
Model model_aux; // un quad para cubrir la ventana auxiliar y mostrar la textura

Shader shader_main; // shader para el objeto principal (drawMain)
Shader shader_aux; // shader para la ventana auxiliar (drawTexture)

// Variables para tracking del mouse
double last_x = 0, last_y = 0;

// Funciones auxiliares para rasterización
void drawPoint(int x, int y, float radius, const glm::vec4& color);
void drawLine(int x1, int y1, int x2, int y2, float width, const glm::vec4& color);
void drawCircle(int center_x, int center_y, float radius, const glm::vec4& color);
glm::vec2 screenToTextureCoords(double x, double y, GLFWwindow* window);

// callbacks del mouse y auxiliares para los callbacks
enum class MouseAction { None, ManipulateView, Draw };
MouseAction mouse_action = MouseAction::None; // qu� hacer en el callback del motion si el bot�n del mouse est� apretado
void mainMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void auxMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

int main() {
	
	// main window (3D view)
	main_window = Window(800, 600, "Main View", true);
	glfwSetCursorPosCallback(main_window, mainMouseMoveCallback);
	glfwSetMouseButtonCallback(main_window, mainMouseButtonCallback);
	main_window.getCamera().model_angle = 2.5;
	
	glClearColor(1.f,1.f,1.f,1.f);
	shader_main = Shader("shaders/main");
	
	image = Image("models/chookity.png",true);
	texture = Texture(image);
	
	model_chookity = Model::loadSingle("models/chookity", Model::fNoTextures);
	
	// aux window (texture image)
	aux_window = Window(512,512, "Texture", true, main_window);
	glfwSetCursorPosCallback(aux_window, auxMouseMoveCallback);
	glfwSetMouseButtonCallback(aux_window, auxMouseButtonCallback);
	
	model_aux = Model::loadSingle("models/texquad", Model::fNoTextures);
	shader_aux = Shader("shaders/quad");
	
	// main loop
	do {
		glfwPollEvents();
		
		glfwMakeContextCurrent(main_window);
		drawMain();
		drawImGui(main_window);
		glFinish();
		glfwSwapBuffers(main_window);
		
		glfwMakeContextCurrent(aux_window);
		drawAux();
		drawImGui(aux_window);
		glFinish();
		glfwSwapBuffers(aux_window);
		
	} while( (not glfwWindowShouldClose(main_window)) and (not glfwWindowShouldClose(aux_window)) );
}


// ===== pasos del renderizado =====

void drawMain() {
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	texture.bind();
	shader_main.use();
	setMatrixes(main_window, shader_main);
	shader_main.setLight(glm::vec4{-1.f,1.f,4.f,1.f}, glm::vec3{1.f,1.f,1.f}, 0.35f);
	shader_main.setMaterial(model_chookity.material);
	shader_main.setBuffers(model_chookity.buffers);
	model_chookity.buffers.draw();
}

void drawAux() {
	glDisable(GL_DEPTH_TEST);
	texture.bind();
	shader_aux.use();
	shader_aux.setMatrixes(glm::mat4{1.f}, glm::mat4{1.f}, glm::mat4{1.f});
	shader_aux.setBuffers(model_aux.buffers);
	model_aux.buffers.draw();
}

void drawBack() {
	glfwMakeContextCurrent(main_window);
	glDisable(GL_MULTISAMPLE);

	/// @ToDo: Parte 2: renderizar el modelo en 3d con un nuevo shader de forma 
	///                 que queden las coordenadas de textura de cada fragmento
	///                 en el back-buffer de color
	
	glEnable(GL_MULTISAMPLE);
	glFlush();
	glFinish();
}

void drawImGui(Window &window) {
	if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) return;
	// settings sub-window
	window.ImGuiDialog("Settings",[&](){
		ImGui::SliderFloat("Radius",&radius,1,50);
		ImGui::ColorEdit4("Color",&(color[0]),0);
		
		static std::vector<std::pair<const char *, ImVec4>> pallete = { // colores predefindos
			{"white" , {1.f,1.f,1.f,1.f}},
			{"pink"  , {0.749f,0.49f,0.498f,1.f}},
			{"yellow", {0.965f,0.729f,0.106f,1.f}},
			{"black" , {0.f,0.f,0.f,1.f}} };
		
		ImGui::Text("Pallete:");
		for (auto &p : pallete) {
			ImGui::SameLine();
			if (ImGui::ColorButton(p.first, p.second))
				color[0] = p.second.x, color[1] = p.second.y, color[2] = p.second.z;
		}
		
		if (ImGui::Button("Reload Image")) {
			image = Image("models/chookity.png",true);
			texture.update(image);
		}
	});
}



// ===== callbacks de la ventana auxiliar (textura) =====

void auxMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;
	if (action==GLFW_PRESS) {
		mouse_action = MouseAction::Draw;
		
		// Obtener coordenadas del mouse
		glfwGetCursorPos(window, &last_x, &last_y);
		
		// Convertir coordenadas de pantalla a coordenadas de textura
		glm::vec2 tex_coords = screenToTextureCoords(last_x, last_y, window);
		
		// Pintar un punto en la imagen
		drawPoint(tex_coords.x, tex_coords.y, radius, color);
		
		// Actualizar la textura en la GPU
		texture.update(image);
		
	} else {
		mouse_action = MouseAction::None;
	}
}

void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse_action!=MouseAction::Draw) return;
	
	// Convertir coordenadas actuales y anteriores a coordenadas de textura
	glm::vec2 current_tex_coords = screenToTextureCoords(xpos, ypos, window);
	glm::vec2 last_tex_coords = screenToTextureCoords(last_x, last_y, window);
	
	// Pintar una línea entre los dos puntos
	drawLine(last_tex_coords.x, last_tex_coords.y, 
	         current_tex_coords.x, current_tex_coords.y, 
	         radius * 2.0f, color);
	
	// Actualizar la textura en la GPU
	texture.update(image);
	
	// Guardar las coordenadas actuales para el siguiente frame
	last_x = xpos;
	last_y = ypos;
}


// ===== callbacks de la ventana principal (vista 3D) =====

void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;
	if (action==GLFW_PRESS) {
		if (mods!=0 or button==GLFW_MOUSE_BUTTON_RIGHT) {
			mouse_action = MouseAction::ManipulateView;
			common_callbacks::mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, action, mods);
			return;
		}
		
		mouse_action = MouseAction::Draw;
		
		/// @ToDo: Parte 2: pintar un punto de radio "radius" en la imagen
		///                 "image" que se usa como textura
		
	} else {
		if (mouse_action==MouseAction::ManipulateView)
			common_callbacks::mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, action, mods);
		mouse_action = MouseAction::None;
	}
}

void mainMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse_action!=MouseAction::Draw) {
		if (mouse_action==MouseAction::ManipulateView);
			common_callbacks::mouseMoveCallback(window,xpos,ypos);
		return; 
	}
	
	/// @ToDo: Parte 2: pintar un segmento de ancho "2*radius" en la imagen
	///                 "image" que se usa como textura
	
}

// ===== Funciones auxiliares para rasterización =====

glm::vec2 screenToTextureCoords(double x, double y, GLFWwindow* window) {
	BufferSize bs = getBufferSize(window);
	// Convertir coordenadas de pantalla a coordenadas de textura
	// La ventana auxiliar muestra la textura completa, así que es una conversión directa
	float tex_x = (float)x / bs.width;
	float tex_y = 1.0f - (float)y / bs.height; // Flip Y coordinate
	
	// Convertir a coordenadas de píxeles en la imagen
	int pixel_x = (int)(tex_x * image.GetWidth());
	int pixel_y = (int)(tex_y * image.GetHeight());
	
	return glm::vec2(pixel_x, pixel_y);
}

void drawCircle(int center_x, int center_y, float radius, const glm::vec4& color) {
	int min_x = std::max(0, (int)(center_x - radius));
	int max_x = std::min(image.GetWidth() - 1, (int)(center_x + radius));
	int min_y = std::max(0, (int)(center_y - radius));
	int max_y = std::min(image.GetHeight() - 1, (int)(center_y + radius));
	
	float radius_sq = radius * radius;
	
	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			float dx = x - center_x;
			float dy = y - center_y;
			float dist_sq = dx * dx + dy * dy;
			
			if (dist_sq <= radius_sq) {
				// Blend with existing color using alpha
				glm::vec4 existing_color = image.GetRGBA(y, x);
				glm::vec4 blended_color = glm::mix(existing_color, color, color.a);
				image.SetRGBA(y, x, blended_color);
			}
		}
	}
}

void drawPoint(int x, int y, float radius, const glm::vec4& color) {
	drawCircle(x, y, radius, color);
}

void drawLine(int x1, int y1, int x2, int y2, float width, const glm::vec4& color) {
	// Bresenham's line algorithm with thickness
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int sx = x1 < x2 ? 1 : -1;
	int sy = y1 < y2 ? 1 : -1;
	int err = dx - dy;
	
	int x = x1, y = y1;
	
	while (true) {
		// Draw a circle at each point along the line
		drawCircle(x, y, width / 2.0f, color);
		
		if (x == x2 && y == y2) break;
		
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x += sx;
		}
		if (e2 < dx) {
			err += dx;
			y += sy;
		}
	}
}
