#version 130

// Represents light source passed to shader.
struct LightSource
{
    // (x, y, z, 1) means positioned light.
    // (x, y, z, 0) means directed light.
    vec4 position;
    vec4 diffuse;
    vec4 specular;
};

// Keeps intermediate result.
struct LightIntensity
{
    float diffuse;
    float specular;
};

// Constant Phong lighting model shininess.
const float kShininess = 30.0;
const float kAmbientLight = 0.15;

in vec3 v_normal;
in vec2 v_texture_uv;
in vec3 v_pos_in_world_space;

uniform sampler2D u_color_map;
uniform vec4 u_color_map_rect;

uniform LightSource u_light0;
uniform LightSource u_light1;
uniform vec3 u_viewer_position;

out vec4 out_fragColor;

LightIntensity calculateIntensity(LightSource light)
{
    vec3 direction_to_viewer = normalize(u_viewer_position - v_pos_in_world_space);
    vec3 normal = normalize(v_normal);

    // Calculate direction_to_light for both directed and undirected light sources.
    vec3 direction_to_light = normalize(vec3(light.position) - light.position.w * v_pos_in_world_space);
    vec3 reflect_direction = normalize(reflect(-direction_to_light, normal));
	
    LightIntensity result;
    result.diffuse = max(dot(normal, direction_to_light), 0.0);
    result.specular = pow(max(dot(reflect_direction, direction_to_viewer), 0.0), kShininess);

	// Clamp intensity to [0..1].
    result.diffuse = clamp(result.diffuse, 0.0, 1.0);
    result.specular = clamp(result.specular, 0.0, 1.0);

    return result;
}

vec4 getColorMapColor()
{
	float x = u_color_map_rect.x;
	float y = u_color_map_rect.y;
	float width = u_color_map_rect.z;
	float height = u_color_map_rect.w;
	vec2 texture_uv = vec2(x + width * v_texture_uv.x, y + height * v_texture_uv.y);
    return texture(u_color_map, texture_uv);
}

void main()
{
	LightIntensity light0_intensity = calculateIntensity(u_light0);
	LightIntensity light1_intensity = calculateIntensity(u_light1);

    vec4 diffuse_color = getColorMapColor();
    vec4 diffuse_intensity = u_light0.diffuse * light0_intensity.diffuse + u_light1.diffuse * light1_intensity.diffuse;
    vec4 specular_intensity = u_light0.specular * light0_intensity.specular + u_light1.specular * light1_intensity.specular;

    out_fragColor = diffuse_color * (kAmbientLight + diffuse_intensity + specular_intensity);
}
