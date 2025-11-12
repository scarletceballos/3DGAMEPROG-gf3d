#include <math.h>

#include "simple_logger.h"

#include "gf3d_vgraphics.h"
#include "gf3d_camera.h"
#include "gfc_input.h"
#include "gfc_vector.h"

// Internal camera state
static Camera gf3d_camera = {0};

// complicated work that didnt matter but could be used for something
/* void gf3d_camera_set_view(gfc_vector3d eye, gfc_vector3d target, gfc_vector3d up){
    gf3d_camera_get_view_matrix(GFC_matrix4 out, GFC_Vector3D eye, GFC_Vector3D target, GFC_Vector3D up)
    gf3d_vgraphics_set_view(gf3d_camera.cameraMat);
} */

/* void gf3d_camera_get_view_matrix(GFC_matrix4 out, GFC_Vector3D eye, GFC_Vector3D target, GFC_Vector3D up){
    gfc_vector3d zaxis,yaxis,xaxis;
    
    gfc_matrix4_identity(out);
    
    gfc_vector3d_sub(zaxis,eye,target);
    gfc_vector3d_normalize(&zaxis);
    
    xaxis = gfc_vector3d_cross_product(up,zaxis);
    gfc_vector3d_normalize(&xaxis);
    
    yaxis = gfc_vector3d_cross_product(zaxis,xaxis);
    
    // 4x4 view matrix from right,up,forward,and eye position vectors
    
    out[0][0] = xaxis.x;
    out[0][1] = yaxis.x;
    out[0][2] = zaxis.x;
    
    out[2][0] = xaxis.y;
    out[2][1] = xaxis.y;
    out[2][2] = xaxis.y;
    
    out[3][0] = -gfc_vector3d_dot_product(xaxis,eye);
    out[3][1] = -gfc_vector3d_dot_product(yaxis,eye);
    out[3][2] = -gfc_vector3d_dot_product(zaxis,eye);
    
    vec4(-dot( xaxis, eye ), -dot( yaxis, eye ), -dot( zaxis, eye ), 1)
} */

static int s_free_mode = 0; // 0 fixed, 1 free
static GFC_Vector3D s_camera_pos = {0, -1, 55};
static GFC_Vector3D s_target_pos = {0, 0, 4};
static float s_move_step = 0.1f;
static float s_rotate_step = 0.05f;
static float s_follow_distance = 20.0f;
static float s_follow_height = 10.0f;
static float s_camera_angle = 0.0f;
static int s_reset_freeze_frames = 0;
static const GFC_Vector3D s_initial_pos = {0, -1, 55};
static const GFC_Vector3D s_initial_look_target = {0, 0, 4};

// out = a + b * scale
static inline void gfc_vec3d_ma(GFC_Vector3D *out, GFC_Vector3D a, GFC_Vector3D b, float scale)
{
    if (!out) return;
    out->x = a.x + b.x * scale;
    out->y = a.y + b.y * scale;
    out->z = a.z + b.z * scale;
}

void gf3d_camera_update_view()
{
    GFC_Vector3D xaxis,yaxis,zaxis,position;
    float cosPitch = cos(gf3d_camera.rotation.x);
    float sinPitch = sin(gf3d_camera.rotation.x);
    float cosYaw = cos(gf3d_camera.rotation.z);
    float sinYaw = sin(gf3d_camera.rotation.z); 

    position.x = gf3d_camera.position.x;
    position.y = -gf3d_camera.position.z;        //inverting for Z-up(?)
    position.z = gf3d_camera.position.y;
    gfc_matrix4_identity(gf3d_camera.cameraMat);

    gfc_vector3d_set(xaxis, cosYaw,0,-sinYaw);
    gfc_vector3d_set(yaxis, sinYaw * sinPitch,cosPitch,cosYaw * sinPitch);
    gfc_vector3d_set(zaxis, sinYaw * cosPitch,-sinPitch,cosPitch * cosYaw);
    
    gf3d_camera.cameraMat[0][0] = xaxis.x;
    gf3d_camera.cameraMat[0][1] = yaxis.x;
    gf3d_camera.cameraMat[0][2] = zaxis.x;

    gf3d_camera.cameraMat[1][0] = xaxis.z;
    gf3d_camera.cameraMat[1][1] = yaxis.z;
    gf3d_camera.cameraMat[1][2] = zaxis.z;

    gf3d_camera.cameraMat[2][0] = -xaxis.y;
    gf3d_camera.cameraMat[2][1] = -yaxis.y;
    gf3d_camera.cameraMat[2][2] = -zaxis.y;

    gf3d_camera.cameraMat[3][0] = gfc_vector3d_dot_product(xaxis, position);
    gf3d_camera.cameraMat[3][1] = gfc_vector3d_dot_product(yaxis, position);
    gf3d_camera.cameraMat[3][2] = gfc_vector3d_dot_product(zaxis, position);
    gf3d_vgraphics_set_view(gf3d_camera.cameraMat);
}

void gf3d_camera_get_view_mat4(GFC_Matrix4 *view)
{
    if (!view) return;
    gfc_matrix4_copy((*view), gf3d_camera.cameraMat);
}

void gf3d_camera_set_view_mat4(GFC_Matrix4 *view)
{
    if (!view) return;
    gfc_matrix4_copy(gf3d_camera.cameraMat, (*view));
    gf3d_vgraphics_set_view(gf3d_camera.cameraMat);
}

GFC_Vector3D gf3d_camera_get_position()
{
    GFC_Vector3D position;
    gfc_vector3d_negate(position,gf3d_camera.position);
    return position;
}

void gf3d_camera_set_position(GFC_Vector3D position)
{
    gf3d_camera.position.x = -position.x;
    gf3d_camera.position.y = -position.y;
    gf3d_camera.position.z = -position.z;
}

void gf3d_camera_set_rotation(GFC_Vector3D rotation)
{
    gfc_angle_clamp_radians(&rotation.x);
    gf3d_camera.rotation.x = -rotation.x;
    gf3d_camera.rotation.y = -rotation.y;
    gf3d_camera.rotation.z = -rotation.z;
}

void gf3d_camera_set_scale(GFC_Vector3D scale)
{
    if (!scale.x)gf3d_camera.scale.x = 0;
    else gf3d_camera.scale.x = 1/scale.x;
    if (!scale.y)gf3d_camera.scale.y = 0;
    else gf3d_camera.scale.y = 1/scale.y;
    if (!scale.z)gf3d_camera.scale.z = 0;
    else gf3d_camera.scale.z = 1/scale.z;
}

void gf3d_camera_look_at(GFC_Vector3D target,const GFC_Vector3D *position)
{
    GFC_Vector3D angles,pos;
    GFC_Vector3D delta;
    if (position)
    {
        gfc_vector3d_copy(pos,(*position));
        gf3d_camera_set_position(pos);
    }
    else
    {
        pos = gf3d_camera_get_position();
    }
    gf3d_camera.lookTargetPosition = target;
    gfc_vector3d_sub(delta,target,pos);
    gfc_vector3d_angles (delta, &angles);

    // angled camera
    angles.z -= GFC_HALF_PI;
    angles.x -= GFC_PI;
    gf3d_camera_set_rotation(angles);
}

void gf3d_camera_set_distance_to_target(float set)
{
    GFC_Vector3D pos = gf3d_camera_get_position();
    GFC_Vector3D dir;
    gfc_vector3d_sub(dir, pos, gf3d_camera.lookTargetPosition);
    float len = gfc_vector3d_magnitude(dir);
    if (len <= 0.0001f) return;
    gfc_vector3d_scale(dir, dir, set/len);
    gfc_vector3d_add(pos, gf3d_camera.lookTargetPosition, dir);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_set_move_step(float step)
{
    // Allow zero speed to disable movement; clamp negatives to 0
    if (step < 0) step = 0;
    s_move_step = step;
}

void gf3d_camera_set_rotate_step(float step)
{
    if (step <= 0) return;
    s_rotate_step = step;
}

GFC_Vector3D gf3d_camera_get_angles()
{
    GFC_Vector3D a = {-gf3d_camera.rotation.x, -gf3d_camera.rotation.y, -gf3d_camera.rotation.z};
    return a;
}

void gf3d_camera_get_view_vectors(GFC_Vector3D *forward, GFC_Vector3D *right, GFC_Vector3D *up)
{
    GFC_Vector3D a = gf3d_camera_get_angles();
    GFC_Vector3D f,r,u;
    gfc_vector3d_angle_vectors(a, &f, &r, &u);
    if (forward) *forward = f;
    if (right) *right = r;
    if (up) *up = u;
}

void gf3d_camera_yaw(float magnitude)
{
    GFC_Vector3D a = gf3d_camera_get_angles();
    a.z += magnitude;
    gf3d_camera_set_rotation(a);
}

void gf3d_camera_pitch(float magnitude)
{
    GFC_Vector3D a = gf3d_camera_get_angles();
    a.x += magnitude;
    gf3d_camera_set_rotation(a);
}

void gf3d_camera_roll(float magnitude)
{
    GFC_Vector3D a = gf3d_camera_get_angles();
    a.y += magnitude;
    gf3d_camera_set_rotation(a);
}

void gf3d_camera_fly_forward(float magnitude)
{
    GFC_Vector3D f; gf3d_camera_get_view_vectors(&f, NULL, NULL);
    GFC_Vector3D pos = gf3d_camera_get_position();
    gfc_vec3d_ma(&pos, pos, f, magnitude);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_fly_right(float magnitude)
{
    GFC_Vector3D r; gf3d_camera_get_view_vectors(NULL, &r, NULL);
    GFC_Vector3D pos = gf3d_camera_get_position();
    gfc_vec3d_ma(&pos, pos, r, magnitude);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_fly_up(float magnitude)
{
    GFC_Vector3D u; gf3d_camera_get_view_vectors(NULL, NULL, &u);
    GFC_Vector3D pos = gf3d_camera_get_position();
    gfc_vec3d_ma(&pos, pos, u, magnitude);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_walk_forward(float magnitude)
{
    GFC_Vector3D f; gf3d_camera_get_view_vectors(&f, NULL, NULL);
    f.z = 0; gfc_vector3d_normalize(&f);
    GFC_Vector3D pos = gf3d_camera_get_position();
    gfc_vec3d_ma(&pos, pos, f, magnitude);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_walk_right(float magnitude)
{
    GFC_Vector3D r; gf3d_camera_get_view_vectors(NULL, &r, NULL);
    r.z = 0; gfc_vector3d_normalize(&r);
    GFC_Vector3D pos = gf3d_camera_get_position();
    gfc_vec3d_ma(&pos, pos, r, magnitude);
    gf3d_camera_set_position(pos);
}

void gf3d_camera_move_up(float magnitude)
{
    GFC_Vector3D pos = gf3d_camera_get_position();
    pos.z += magnitude;
    gf3d_camera_set_position(pos);
}

GFC_Vector3D gf3d_camera_get_direction()
{
    GFC_Vector3D f; gf3d_camera_get_view_vectors(&f, NULL, NULL);
    return f;
}

GFC_Vector3D gf3d_camera_get_right()
{
    GFC_Vector3D r; gf3d_camera_get_view_vectors(NULL, &r, NULL);
    return r;
}

GFC_Vector3D gf3d_camera_get_up()
{
    GFC_Vector3D u; gf3d_camera_get_view_vectors(NULL, NULL, &u);
    return u;
}

void gf3d_camera_set_look_target(GFC_Vector3D target)
{
    gf3d_camera.lookTargetPosition = target;
}

GFC_Vector3D gf3d_camera_get_look_target()
{
    return gf3d_camera.lookTargetPosition;
}

void gf3d_camera_toggle_free_look()
{
    s_free_mode = !s_free_mode;
}

void gf3d_camera_enable_free_look(Uint8 enable)
{
    s_free_mode = enable ? 1 : 0;
}

void gf3d_camera_set_auto_pan(Bool enable)
{
    gf3d_camera.autoPan = enable ? 1 : 0;
}

Bool gf3d_camera_free_look_enabled()
{
    return s_free_mode ? 1 : 0;
}

float gf3d_camera_get_move_step()
{
    return s_move_step;
}

void gf3d_camera_reset_to_initial()
{
    s_free_mode = 0;
    s_camera_pos = s_initial_pos;
    s_target_pos = s_initial_look_target;
    s_camera_angle = 0.0f;
    gf3d_camera_set_position(s_camera_pos);
    gf3d_camera_look_at(s_target_pos, NULL);
    s_reset_freeze_frames = 10;
}

void gf3d_camera_init_controls()
{
    // Camera toggle: only top-row 0
    gfc_input_command_clear_keys("cameratoggle");
    gfc_input_command_add_key("cameratoggle","0");
    // Reset
    gfc_input_command_add_key("camreset","r");
    // Speed +/-
    gfc_input_command_add_key("cameraspeedup","=");
    gfc_input_command_add_key("cameraspeedup","KP_+");
    gfc_input_command_add_key("cameraslowdown","-");
    gfc_input_command_add_key("cameraslowdown","KP_-");
    // Movement defaults (arrows + WASD + space/shift)
    gfc_input_command_add_key("left","LEFT");
    gfc_input_command_add_key("right","RIGHT");
    gfc_input_command_add_key("up","UP");
    gfc_input_command_add_key("down","DOWN");
    gfc_input_command_add_key("jump","SPACE");
    gfc_input_command_add_key("crouch","LSHIFT");
    gfc_input_command_add_key("walkleft","a");
    gfc_input_command_add_key("walkright","d");
    gfc_input_command_add_key("walkforward","w");
    gfc_input_command_add_key("walkback","s");
    // Camera panning (Q/E for left/right rotation)
    gfc_input_command_add_key("panleft","q");
    gfc_input_command_add_key("panright","e");
}

void gf3d_camera_controls_update()
{
    float move = 5.0f;
    float turn = 0.05f;
    float pitch_move = 0.1f;
    float yaw_change = 0.0f;
    float pitch_change = 0.0f;
    GFC_Vector3D movement = {0};
    GFC_Vector3D dir = {0};
    
    // Toggle free/fixed
    if (gfc_input_command_pressed("cameratoggle"))
    {
        s_free_mode = !s_free_mode;
        slog("Camera mode TOGGLED: %s", s_free_mode ? "FREE" : "FIXED");
    }
    
    // Reset to initial
    if (gfc_input_command_pressed("camreset") || gfc_input_key_pressed("r"))
    {
        gf3d_camera_reset_to_initial();
        slog("Camera reset to initial position and fixed mode");
        return;
    }

    // Speed adjust
    const float speed_min = 0.0f;
    const float speed_max = 1.0f;
    float old = s_move_step;
    if (gfc_input_command_pressed("cameraspeedup") || gfc_input_key_pressed("=") || gfc_input_key_pressed("KP_+"))
    {
        s_move_step *= 1.5f; 
        if (s_move_step > speed_max) s_move_step = speed_max;
    }
    if (gfc_input_command_pressed("cameraslowdown") || gfc_input_key_pressed("-") || gfc_input_key_pressed("KP_-"))
    {
        s_move_step /= 1.5f; 
        if (s_move_step < speed_min) s_move_step = speed_min;
    }
    if (s_move_step != old)
    {
        slog("Camera speed changed: %.6f", s_move_step);
    }

    if (s_reset_freeze_frames > 0) {
        s_reset_freeze_frames--;
        return;
    }

    if (!s_free_mode) {
        // In fixed mode, show hint if movement attempted
        if (gfc_input_command_down("left") || gfc_input_command_down("right") || 
            gfc_input_command_down("up") || gfc_input_command_down("down") ||
            gfc_input_key_down("LEFT") || gfc_input_key_down("RIGHT") ||
            gfc_input_key_down("UP") || gfc_input_key_down("DOWN"))
        {
            static int fixed_hint_counter = 0;
            if ((fixed_hint_counter++ % 60) == 0)
            {
                slog("Camera is FIXED. Press '0' to enable FREE camera mode.");
            }
        }
        return;
    }

    // Free mode
    if (s_move_step <= 0.0f) return; // No movement if speed is zero

    // Calculate direction from camera to target
    gfc_vector3d_sub(dir, s_target_pos, s_camera_pos);
    gfc_vector3d_normalize(&dir);
    
    // Handle movement input (WASD/Arrow keys move both camera and target)
    if (gfc_input_command_down("up") || gfc_input_command_down("walkforward") || gfc_input_key_down("UP") || gfc_input_key_down("w"))
    {
        movement.y += move * s_move_step;
    }
    if (gfc_input_command_down("down") || gfc_input_command_down("walkback") || gfc_input_key_down("DOWN") || gfc_input_key_down("s"))
    {
        movement.y -= move * s_move_step;
    }
    if (gfc_input_command_down("right") || gfc_input_command_down("walkright") || gfc_input_key_down("RIGHT") || gfc_input_key_down("d"))
    {
        movement.x += move * s_move_step;
    }
    if (gfc_input_command_down("left") || gfc_input_command_down("walkleft") || gfc_input_key_down("LEFT") || gfc_input_key_down("a"))
    {
        movement.x -= move * s_move_step;
    }
    
    // Apply forward/back movement along view direction
    if (movement.y != 0.0f) {
        GFC_Vector3D forward_movement;
        gfc_vector3d_scale(forward_movement, dir, movement.y);
        gfc_vector3d_add(s_camera_pos, s_camera_pos, forward_movement);
        gfc_vector3d_add(s_target_pos, s_target_pos, forward_movement);
    }
    
    // Apply left/right movement perpendicular to view direction
    if (movement.x != 0.0f) {
        GFC_Vector3D right_dir = dir;
        gfc_vector3d_rotate_about_z(&right_dir, -GFC_HALF_PI); // Rotate 90 degrees right
        gfc_vector3d_scale(right_dir, right_dir, movement.x);
        gfc_vector3d_add(s_camera_pos, s_camera_pos, right_dir);
        gfc_vector3d_add(s_target_pos, s_target_pos, right_dir);
    }
    
    // Handle camera panning (rotation around target)
    if (gfc_input_key_down("q") || gfc_input_command_down("panleft"))
    {
        yaw_change += turn;
    }
    if (gfc_input_key_down("e") || gfc_input_command_down("panright"))
    {
        yaw_change -= turn;
    }
    if (gfc_input_command_down("jump") || gfc_input_key_down("SPACE"))
    {
        pitch_change += pitch_move * s_move_step;
    }
    if (gfc_input_command_down("crouch") || gfc_input_key_down("LSHIFT"))
    {
        pitch_change -= pitch_move * s_move_step;
    }
    
    // Apply vertical target movement, itch
    if (pitch_change != 0.0f) {
        s_target_pos.z += pitch_change;
    }
    
    // Apply horizontal camera rotation yaw
    if (yaw_change != 0.0f) {
        GFC_Vector3D offset;
        gfc_vector3d_sub(offset, s_camera_pos, s_target_pos);
        gfc_vector3d_rotate_about_z(&offset, yaw_change);
        gfc_vector3d_add(s_camera_pos, s_target_pos, offset);
        s_camera_angle += yaw_change;
    }
    
    // Update camera position and look direction
    gf3d_camera_set_position(s_camera_pos);
    gf3d_camera_look_at(s_target_pos, NULL);
}

