#include "rendering/vector_font.hpp"
#include "rendering/primitives.hpp"
#include <cstring>

namespace rendering {

// Glyph data storage - packed x,y coordinates in 0-100 space
// Each pair of bytes is (x, y)

// Digit 0: Hexagonal shape
static const uint8_t GLYPH_0_S0[] = {20,10, 80,10, 95,25, 95,75, 80,90, 20,90, 5,75, 5,25, 20,10};
static const GlyphStroke GLYPH_0_STROKES[] = {{GLYPH_0_S0, 9}};
static const Glyph GLYPH_0 = {GLYPH_0_STROKES, 1};

// Digit 1: Simple vertical with base
static const uint8_t GLYPH_1_S0[] = {30,20, 50,10, 50,90};
static const uint8_t GLYPH_1_S1[] = {30,90, 70,90};
static const GlyphStroke GLYPH_1_STROKES[] = {{GLYPH_1_S0, 3}, {GLYPH_1_S1, 2}};
static const Glyph GLYPH_1 = {GLYPH_1_STROKES, 2};

// Digit 2
static const uint8_t GLYPH_2_S0[] = {10,25, 25,10, 75,10, 90,25, 90,40, 10,75, 10,90, 90,90};
static const GlyphStroke GLYPH_2_STROKES[] = {{GLYPH_2_S0, 8}};
static const Glyph GLYPH_2 = {GLYPH_2_STROKES, 1};

// Digit 3
static const uint8_t GLYPH_3_S0[] = {10,10, 80,10, 90,20, 90,40, 75,50};
static const uint8_t GLYPH_3_S1[] = {45,50, 75,50};
static const uint8_t GLYPH_3_S2[] = {75,50, 90,60, 90,80, 80,90, 10,90};
static const GlyphStroke GLYPH_3_STROKES[] = {{GLYPH_3_S0, 5}, {GLYPH_3_S1, 2}, {GLYPH_3_S2, 5}};
static const Glyph GLYPH_3 = {GLYPH_3_STROKES, 3};

// Digit 4
static const uint8_t GLYPH_4_S0[] = {70,10, 70,90};
static const uint8_t GLYPH_4_S1[] = {10,60, 90,60};
static const uint8_t GLYPH_4_S2[] = {10,60, 70,10};
static const GlyphStroke GLYPH_4_STROKES[] = {{GLYPH_4_S0, 2}, {GLYPH_4_S1, 2}, {GLYPH_4_S2, 2}};
static const Glyph GLYPH_4 = {GLYPH_4_STROKES, 3};

// Digit 5
static const uint8_t GLYPH_5_S0[] = {85,10, 15,10, 10,15, 10,45, 20,50, 75,50, 90,60, 90,80, 75,90, 10,90};
static const GlyphStroke GLYPH_5_STROKES[] = {{GLYPH_5_S0, 10}};
static const Glyph GLYPH_5 = {GLYPH_5_STROKES, 1};

// Digit 6
static const uint8_t GLYPH_6_S0[] = {80,10, 25,10, 10,25, 10,75, 25,90, 75,90, 90,75, 90,55, 75,45, 10,45};
static const GlyphStroke GLYPH_6_STROKES[] = {{GLYPH_6_S0, 10}};
static const Glyph GLYPH_6 = {GLYPH_6_STROKES, 1};

// Digit 7
static const uint8_t GLYPH_7_S0[] = {10,10, 90,10, 90,20, 45,90};
static const uint8_t GLYPH_7_S1[] = {30,50, 70,50};
static const GlyphStroke GLYPH_7_STROKES[] = {{GLYPH_7_S0, 4}, {GLYPH_7_S1, 2}};
static const Glyph GLYPH_7 = {GLYPH_7_STROKES, 2};

// Digit 8
static const uint8_t GLYPH_8_S0[] = {25,10, 75,10, 90,20, 90,40, 75,50, 25,50, 10,40, 10,20, 25,10};
static const uint8_t GLYPH_8_S1[] = {25,50, 75,50, 90,60, 90,80, 75,90, 25,90, 10,80, 10,60, 25,50};
static const GlyphStroke GLYPH_8_STROKES[] = {{GLYPH_8_S0, 9}, {GLYPH_8_S1, 9}};
static const Glyph GLYPH_8 = {GLYPH_8_STROKES, 2};

// Digit 9
static const uint8_t GLYPH_9_S0[] = {90,55, 25,55, 10,45, 10,25, 25,10, 75,10, 90,25, 90,75, 75,90, 20,90};
static const GlyphStroke GLYPH_9_STROKES[] = {{GLYPH_9_S0, 10}};
static const Glyph GLYPH_9 = {GLYPH_9_STROKES, 1};

// Colon - two diamond dots
static const uint8_t GLYPH_COLON_S0[] = {50,25, 58,33, 50,41, 42,33, 50,25};
static const uint8_t GLYPH_COLON_S1[] = {50,59, 58,67, 50,75, 42,67, 50,59};
static const GlyphStroke GLYPH_COLON_STROKES[] = {{GLYPH_COLON_S0, 5}, {GLYPH_COLON_S1, 5}};
static const Glyph GLYPH_COLON = {GLYPH_COLON_STROKES, 2};

// Minus
static const uint8_t GLYPH_MINUS_S0[] = {15,50, 85,50};
static const GlyphStroke GLYPH_MINUS_STROKES[] = {{GLYPH_MINUS_S0, 2}};
static const Glyph GLYPH_MINUS = {GLYPH_MINUS_STROKES, 1};

// Period
static const uint8_t GLYPH_PERIOD_S0[] = {50,80, 58,85, 50,90, 42,85, 50,80};
static const GlyphStroke GLYPH_PERIOD_STROKES[] = {{GLYPH_PERIOD_S0, 5}};
static const Glyph GLYPH_PERIOD = {GLYPH_PERIOD_STROKES, 1};

// Forward slash
static const uint8_t GLYPH_SLASH_S0[] = {85,10, 15,90};
static const GlyphStroke GLYPH_SLASH_STROKES[] = {{GLYPH_SLASH_S0, 2}};
static const Glyph GLYPH_SLASH = {GLYPH_SLASH_STROKES, 1};

// Degree
static const uint8_t GLYPH_DEGREE_S0[] = {30,10, 70,10, 85,25, 85,40, 70,55, 30,55, 15,40, 15,25, 30,10};
static const GlyphStroke GLYPH_DEGREE_STROKES[] = {{GLYPH_DEGREE_S0, 9}};
static const Glyph GLYPH_DEGREE = {GLYPH_DEGREE_STROKES, 1};

// Percent
static const uint8_t GLYPH_PERCENT_S0[] = {85,10, 15,90};
static const uint8_t GLYPH_PERCENT_S1[] = {15,10, 35,10, 40,15, 40,30, 35,35, 15,35, 10,30, 10,15, 15,10};
static const uint8_t GLYPH_PERCENT_S2[] = {65,65, 85,65, 90,70, 90,85, 85,90, 65,90, 60,85, 60,70, 65,65};
static const GlyphStroke GLYPH_PERCENT_STROKES[] = {{GLYPH_PERCENT_S0, 2}, {GLYPH_PERCENT_S1, 9}, {GLYPH_PERCENT_S2, 9}};
static const Glyph GLYPH_PERCENT = {GLYPH_PERCENT_STROKES, 3};

// Letter A
static const uint8_t GLYPH_A_S0[] = {5,90, 5,75, 20,10, 80,10, 95,75, 95,90};
static const uint8_t GLYPH_A_S1[] = {20,60, 80,60};
static const GlyphStroke GLYPH_A_STROKES[] = {{GLYPH_A_S0, 6}, {GLYPH_A_S1, 2}};
static const Glyph GLYPH_A = {GLYPH_A_STROKES, 2};

// Letter B
static const uint8_t GLYPH_B_S0[] = {10,10, 10,90, 75,90, 90,75, 90,55, 75,45};
static const uint8_t GLYPH_B_S1[] = {10,45, 75,45, 90,35, 90,20, 75,10, 10,10};
static const GlyphStroke GLYPH_B_STROKES[] = {{GLYPH_B_S0, 6}, {GLYPH_B_S1, 6}};
static const Glyph GLYPH_B = {GLYPH_B_STROKES, 2};

// Letter C
static const uint8_t GLYPH_C_S0[] = {90,25, 75,10, 25,10, 10,25, 10,75, 25,90, 75,90, 90,75};
static const GlyphStroke GLYPH_C_STROKES[] = {{GLYPH_C_S0, 8}};
static const Glyph GLYPH_C = {GLYPH_C_STROKES, 1};

// Letter D
static const uint8_t GLYPH_D_S0[] = {10,10, 10,90, 70,90, 90,70, 90,30, 70,10, 10,10};
static const GlyphStroke GLYPH_D_STROKES[] = {{GLYPH_D_S0, 7}};
static const Glyph GLYPH_D = {GLYPH_D_STROKES, 1};

// Letter E
static const uint8_t GLYPH_E_S0[] = {90,10, 10,10, 10,90, 90,90};
static const uint8_t GLYPH_E_S1[] = {10,50, 70,50};
static const GlyphStroke GLYPH_E_STROKES[] = {{GLYPH_E_S0, 4}, {GLYPH_E_S1, 2}};
static const Glyph GLYPH_E = {GLYPH_E_STROKES, 2};

// Letter F
static const uint8_t GLYPH_F_S0[] = {90,10, 10,10, 10,90};
static const uint8_t GLYPH_F_S1[] = {10,50, 70,50};
static const GlyphStroke GLYPH_F_STROKES[] = {{GLYPH_F_S0, 3}, {GLYPH_F_S1, 2}};
static const Glyph GLYPH_F = {GLYPH_F_STROKES, 2};

// Letter G
static const uint8_t GLYPH_G_S0[] = {90,25, 75,10, 25,10, 10,25, 10,75, 25,90, 75,90, 90,75, 90,50, 50,50};
static const GlyphStroke GLYPH_G_STROKES[] = {{GLYPH_G_S0, 10}};
static const Glyph GLYPH_G = {GLYPH_G_STROKES, 1};

// Letter H
static const uint8_t GLYPH_H_S0[] = {10,10, 10,90};
static const uint8_t GLYPH_H_S1[] = {90,10, 90,90};
static const uint8_t GLYPH_H_S2[] = {10,50, 90,50};
static const GlyphStroke GLYPH_H_STROKES[] = {{GLYPH_H_S0, 2}, {GLYPH_H_S1, 2}, {GLYPH_H_S2, 2}};
static const Glyph GLYPH_H = {GLYPH_H_STROKES, 3};

// Letter I
static const uint8_t GLYPH_I_S0[] = {30,10, 70,10};
static const uint8_t GLYPH_I_S1[] = {50,10, 50,90};
static const uint8_t GLYPH_I_S2[] = {30,90, 70,90};
static const GlyphStroke GLYPH_I_STROKES[] = {{GLYPH_I_S0, 2}, {GLYPH_I_S1, 2}, {GLYPH_I_S2, 2}};
static const Glyph GLYPH_I = {GLYPH_I_STROKES, 3};

// Letter J
static const uint8_t GLYPH_J_S0[] = {30,10, 90,10};
static const uint8_t GLYPH_J_S1[] = {70,10, 70,75, 55,90, 25,90, 10,75};
static const GlyphStroke GLYPH_J_STROKES[] = {{GLYPH_J_S0, 2}, {GLYPH_J_S1, 5}};
static const Glyph GLYPH_J = {GLYPH_J_STROKES, 2};

// Letter K
static const uint8_t GLYPH_K_S0[] = {10,10, 10,90};
static const uint8_t GLYPH_K_S1[] = {90,10, 10,50, 90,90};
static const GlyphStroke GLYPH_K_STROKES[] = {{GLYPH_K_S0, 2}, {GLYPH_K_S1, 3}};
static const Glyph GLYPH_K = {GLYPH_K_STROKES, 2};

// Letter L
static const uint8_t GLYPH_L_S0[] = {10,10, 10,90, 90,90};
static const GlyphStroke GLYPH_L_STROKES[] = {{GLYPH_L_S0, 3}};
static const Glyph GLYPH_L = {GLYPH_L_STROKES, 1};

// Letter M
static const uint8_t GLYPH_M_S0[] = {5,90, 5,20, 15,10, 50,45, 85,10, 95,20, 95,90};
static const GlyphStroke GLYPH_M_STROKES[] = {{GLYPH_M_S0, 7}};
static const Glyph GLYPH_M = {GLYPH_M_STROKES, 1};

// Letter N
static const uint8_t GLYPH_N_S0[] = {10,90, 10,20, 20,10, 90,80, 90,10};
static const GlyphStroke GLYPH_N_STROKES[] = {{GLYPH_N_S0, 5}};
static const Glyph GLYPH_N = {GLYPH_N_STROKES, 1};

// Letter O (same as 0)
static const uint8_t GLYPH_O_S0[] = {20,10, 80,10, 95,25, 95,75, 80,90, 20,90, 5,75, 5,25, 20,10};
static const GlyphStroke GLYPH_O_STROKES[] = {{GLYPH_O_S0, 9}};
static const Glyph GLYPH_O = {GLYPH_O_STROKES, 1};

// Letter P
static const uint8_t GLYPH_P_S0[] = {10,90, 10,10, 75,10, 90,25, 90,40, 75,55, 10,55};
static const GlyphStroke GLYPH_P_STROKES[] = {{GLYPH_P_S0, 7}};
static const Glyph GLYPH_P = {GLYPH_P_STROKES, 1};

// Letter Q
static const uint8_t GLYPH_Q_S0[] = {20,10, 80,10, 95,25, 95,75, 80,90, 20,90, 5,75, 5,25, 20,10};
static const uint8_t GLYPH_Q_S1[] = {60,65, 95,95};
static const GlyphStroke GLYPH_Q_STROKES[] = {{GLYPH_Q_S0, 9}, {GLYPH_Q_S1, 2}};
static const Glyph GLYPH_Q = {GLYPH_Q_STROKES, 2};

// Letter R
static const uint8_t GLYPH_R_S0[] = {10,90, 10,10, 75,10, 90,25, 90,40, 75,55, 10,55};
static const uint8_t GLYPH_R_S1[] = {55,55, 90,90};
static const GlyphStroke GLYPH_R_STROKES[] = {{GLYPH_R_S0, 7}, {GLYPH_R_S1, 2}};
static const Glyph GLYPH_R = {GLYPH_R_STROKES, 2};

// Letter S
static const uint8_t GLYPH_S_S0[] = {90,25, 75,10, 25,10, 10,25, 10,40, 25,50, 75,50, 90,60, 90,75, 75,90, 25,90, 10,75};
static const GlyphStroke GLYPH_S_STROKES[] = {{GLYPH_S_S0, 12}};
static const Glyph GLYPH_S = {GLYPH_S_STROKES, 1};

// Letter T
static const uint8_t GLYPH_T_S0[] = {10,10, 90,10};
static const uint8_t GLYPH_T_S1[] = {50,10, 50,90};
static const GlyphStroke GLYPH_T_STROKES[] = {{GLYPH_T_S0, 2}, {GLYPH_T_S1, 2}};
static const Glyph GLYPH_T = {GLYPH_T_STROKES, 2};

// Letter U
static const uint8_t GLYPH_U_S0[] = {10,10, 10,75, 25,90, 75,90, 90,75, 90,10};
static const GlyphStroke GLYPH_U_STROKES[] = {{GLYPH_U_S0, 6}};
static const Glyph GLYPH_U = {GLYPH_U_STROKES, 1};

// Letter V
static const uint8_t GLYPH_V_S0[] = {5,10, 50,90, 95,10};
static const GlyphStroke GLYPH_V_STROKES[] = {{GLYPH_V_S0, 3}};
static const Glyph GLYPH_V = {GLYPH_V_STROKES, 1};

// Letter W
static const uint8_t GLYPH_W_S0[] = {5,10, 20,90, 50,55, 80,90, 95,10};
static const GlyphStroke GLYPH_W_STROKES[] = {{GLYPH_W_S0, 5}};
static const Glyph GLYPH_W = {GLYPH_W_STROKES, 1};

// Letter X
static const uint8_t GLYPH_X_S0[] = {10,10, 90,90};
static const uint8_t GLYPH_X_S1[] = {90,10, 10,90};
static const GlyphStroke GLYPH_X_STROKES[] = {{GLYPH_X_S0, 2}, {GLYPH_X_S1, 2}};
static const Glyph GLYPH_X = {GLYPH_X_STROKES, 2};

// Letter Y
static const uint8_t GLYPH_Y_S0[] = {10,10, 50,50, 90,10};
static const uint8_t GLYPH_Y_S1[] = {50,50, 50,90};
static const GlyphStroke GLYPH_Y_STROKES[] = {{GLYPH_Y_S0, 3}, {GLYPH_Y_S1, 2}};
static const Glyph GLYPH_Y = {GLYPH_Y_STROKES, 2};

// Letter Z
static const uint8_t GLYPH_Z_S0[] = {10,10, 90,10, 10,90, 90,90};
static const GlyphStroke GLYPH_Z_STROKES[] = {{GLYPH_Z_S0, 4}};
static const Glyph GLYPH_Z = {GLYPH_Z_STROKES, 1};

// Lowercase letters - angular style matching uppercase
// Lowercase occupies y range 30-90 (x-height) with descenders extending to 115

// Letter a - angular with stem
static const uint8_t GLYPH_a_S0[] = {85,90, 85,45, 70,30, 25,30, 10,45, 10,60, 25,75, 85,75};
static const uint8_t GLYPH_a_S1[] = {85,75, 85,90};
static const GlyphStroke GLYPH_a_STROKES[] = {{GLYPH_a_S0, 8}, {GLYPH_a_S1, 2}};
static const Glyph GLYPH_a = {GLYPH_a_STROKES, 2};

// Letter b - stem with bowl
static const uint8_t GLYPH_b_S0[] = {15,10, 15,90, 70,90, 85,75, 85,45, 70,30, 15,30};
static const GlyphStroke GLYPH_b_STROKES[] = {{GLYPH_b_S0, 7}};
static const Glyph GLYPH_b = {GLYPH_b_STROKES, 1};

// Letter c - open curve
static const uint8_t GLYPH_c_S0[] = {85,40, 70,30, 25,30, 10,45, 10,75, 25,90, 70,90, 85,80};
static const GlyphStroke GLYPH_c_STROKES[] = {{GLYPH_c_S0, 8}};
static const Glyph GLYPH_c = {GLYPH_c_STROKES, 1};

// Letter d - bowl with stem
static const uint8_t GLYPH_d_S0[] = {85,10, 85,90, 30,90, 15,75, 15,45, 30,30, 85,30};
static const GlyphStroke GLYPH_d_STROKES[] = {{GLYPH_d_S0, 7}};
static const Glyph GLYPH_d = {GLYPH_d_STROKES, 1};

// Letter e - angular with crossbar
static const uint8_t GLYPH_e_S0[] = {10,60, 85,60, 85,45, 70,30, 25,30, 10,45, 10,75, 25,90, 70,90, 85,80};
static const GlyphStroke GLYPH_e_STROKES[] = {{GLYPH_e_S0, 10}};
static const Glyph GLYPH_e = {GLYPH_e_STROKES, 1};

// Letter f - hook with crossbar
static const uint8_t GLYPH_f_S0[] = {85,20, 70,10, 45,10, 30,25, 30,90};
static const uint8_t GLYPH_f_S1[] = {15,45, 55,45};
static const GlyphStroke GLYPH_f_STROKES[] = {{GLYPH_f_S0, 5}, {GLYPH_f_S1, 2}};
static const Glyph GLYPH_f = {GLYPH_f_STROKES, 2};

// Letter g - bowl with descender (descender goes to y=115)
static const uint8_t GLYPH_g_S0[] = {85,30, 30,30, 15,45, 15,70, 30,85, 85,85, 85,105, 70,115, 25,115, 10,105};
static const GlyphStroke GLYPH_g_STROKES[] = {{GLYPH_g_S0, 10}};
static const Glyph GLYPH_g = {GLYPH_g_STROKES, 1};

// Letter h - stem with shoulder
static const uint8_t GLYPH_h_S0[] = {15,10, 15,90};
static const uint8_t GLYPH_h_S1[] = {15,45, 30,30, 70,30, 85,45, 85,90};
static const GlyphStroke GLYPH_h_STROKES[] = {{GLYPH_h_S0, 2}, {GLYPH_h_S1, 5}};
static const Glyph GLYPH_h = {GLYPH_h_STROKES, 2};

// Letter i - stem with dot
static const uint8_t GLYPH_i_S0[] = {50,30, 50,90};
static const uint8_t GLYPH_i_S1[] = {50,10, 55,15, 50,20, 45,15, 50,10};
static const GlyphStroke GLYPH_i_STROKES[] = {{GLYPH_i_S0, 2}, {GLYPH_i_S1, 5}};
static const Glyph GLYPH_i = {GLYPH_i_STROKES, 2};

// Letter j - hook with dot (descender to y=115)
static const uint8_t GLYPH_j_S0[] = {60,30, 60,100, 45,115, 20,115};
static const uint8_t GLYPH_j_S1[] = {60,10, 65,15, 60,20, 55,15, 60,10};
static const GlyphStroke GLYPH_j_STROKES[] = {{GLYPH_j_S0, 4}, {GLYPH_j_S1, 5}};
static const Glyph GLYPH_j = {GLYPH_j_STROKES, 2};

// Letter k - stem with angular arms
static const uint8_t GLYPH_k_S0[] = {15,10, 15,90};
static const uint8_t GLYPH_k_S1[] = {80,30, 15,60, 85,90};
static const GlyphStroke GLYPH_k_STROKES[] = {{GLYPH_k_S0, 2}, {GLYPH_k_S1, 3}};
static const Glyph GLYPH_k = {GLYPH_k_STROKES, 2};

// Letter l - simple stem
static const uint8_t GLYPH_l_S0[] = {50,10, 50,90};
static const GlyphStroke GLYPH_l_STROKES[] = {{GLYPH_l_S0, 2}};
static const Glyph GLYPH_l = {GLYPH_l_STROKES, 1};

// Letter m - double arch
static const uint8_t GLYPH_m_S0[] = {10,90, 10,30, 25,30, 40,45, 40,90};
static const uint8_t GLYPH_m_S1[] = {40,45, 55,30, 75,30, 90,45, 90,90};
static const GlyphStroke GLYPH_m_STROKES[] = {{GLYPH_m_S0, 5}, {GLYPH_m_S1, 5}};
static const Glyph GLYPH_m = {GLYPH_m_STROKES, 2};

// Letter n - single arch
static const uint8_t GLYPH_n_S0[] = {15,90, 15,30, 30,30, 70,30, 85,45, 85,90};
static const GlyphStroke GLYPH_n_STROKES[] = {{GLYPH_n_S0, 6}};
static const Glyph GLYPH_n = {GLYPH_n_STROKES, 1};

// Letter o - closed oval
static const uint8_t GLYPH_o_S0[] = {25,30, 75,30, 90,45, 90,75, 75,90, 25,90, 10,75, 10,45, 25,30};
static const GlyphStroke GLYPH_o_STROKES[] = {{GLYPH_o_S0, 9}};
static const Glyph GLYPH_o = {GLYPH_o_STROKES, 1};

// Letter p - stem with bowl (descender to y=115)
static const uint8_t GLYPH_p_S0[] = {15,115, 15,30, 70,30, 85,45, 85,70, 70,85, 15,85};
static const GlyphStroke GLYPH_p_STROKES[] = {{GLYPH_p_S0, 7}};
static const Glyph GLYPH_p = {GLYPH_p_STROKES, 1};

// Letter q - bowl with stem (descender to y=115)
static const uint8_t GLYPH_q_S0[] = {85,115, 85,30, 30,30, 15,45, 15,70, 30,85, 85,85};
static const GlyphStroke GLYPH_q_STROKES[] = {{GLYPH_q_S0, 7}};
static const Glyph GLYPH_q = {GLYPH_q_STROKES, 1};

// Letter r - stem with shoulder
static const uint8_t GLYPH_r_S0[] = {20,90, 20,30};
static const uint8_t GLYPH_r_S1[] = {20,50, 35,35, 60,30, 85,35};
static const GlyphStroke GLYPH_r_STROKES[] = {{GLYPH_r_S0, 2}, {GLYPH_r_S1, 4}};
static const Glyph GLYPH_r = {GLYPH_r_STROKES, 2};

// Letter s - double curve
static const uint8_t GLYPH_s_S0[] = {85,40, 70,30, 30,30, 15,40, 15,50, 30,60, 70,60, 85,70, 85,80, 70,90, 30,90, 15,80};
static const GlyphStroke GLYPH_s_STROKES[] = {{GLYPH_s_S0, 12}};
static const Glyph GLYPH_s = {GLYPH_s_STROKES, 1};

// Letter t - stem with crossbar
static const uint8_t GLYPH_t_S0[] = {40,10, 40,75, 55,90, 80,90};
static const uint8_t GLYPH_t_S1[] = {20,30, 65,30};
static const GlyphStroke GLYPH_t_STROKES[] = {{GLYPH_t_S0, 4}, {GLYPH_t_S1, 2}};
static const Glyph GLYPH_t = {GLYPH_t_STROKES, 2};

// Letter u - open bottom
static const uint8_t GLYPH_u_S0[] = {15,30, 15,75, 30,90, 70,90, 85,75, 85,30};
static const GlyphStroke GLYPH_u_STROKES[] = {{GLYPH_u_S0, 6}};
static const Glyph GLYPH_u = {GLYPH_u_STROKES, 1};

// Letter v - angular
static const uint8_t GLYPH_v_S0[] = {10,30, 50,90, 90,30};
static const GlyphStroke GLYPH_v_STROKES[] = {{GLYPH_v_S0, 3}};
static const Glyph GLYPH_v = {GLYPH_v_STROKES, 1};

// Letter w - double v
static const uint8_t GLYPH_w_S0[] = {5,30, 25,90, 50,50, 75,90, 95,30};
static const GlyphStroke GLYPH_w_STROKES[] = {{GLYPH_w_S0, 5}};
static const Glyph GLYPH_w = {GLYPH_w_STROKES, 1};

// Letter x - crossed
static const uint8_t GLYPH_x_S0[] = {15,30, 85,90};
static const uint8_t GLYPH_x_S1[] = {85,30, 15,90};
static const GlyphStroke GLYPH_x_STROKES[] = {{GLYPH_x_S0, 2}, {GLYPH_x_S1, 2}};
static const Glyph GLYPH_x = {GLYPH_x_STROKES, 2};

// Letter y - v with descender (descender to y=115)
static const uint8_t GLYPH_y_S0[] = {15,30, 50,75};
static const uint8_t GLYPH_y_S1[] = {85,30, 50,75, 35,100, 20,115};
static const GlyphStroke GLYPH_y_STROKES[] = {{GLYPH_y_S0, 2}, {GLYPH_y_S1, 4}};
static const Glyph GLYPH_y = {GLYPH_y_STROKES, 2};

// Letter z - angular
static const uint8_t GLYPH_z_S0[] = {15,30, 85,30, 15,90, 85,90};
static const GlyphStroke GLYPH_z_STROKES[] = {{GLYPH_z_S0, 4}};
static const Glyph GLYPH_z = {GLYPH_z_STROKES, 1};

const Glyph* getGlyph(char c) {
    if (c >= '0' && c <= '9') {
        static const Glyph* DIGITS[] = {
            &GLYPH_0, &GLYPH_1, &GLYPH_2, &GLYPH_3, &GLYPH_4,
            &GLYPH_5, &GLYPH_6, &GLYPH_7, &GLYPH_8, &GLYPH_9
        };
        return DIGITS[c - '0'];
    }

    if (c >= 'A' && c <= 'Z') {
        static const Glyph* LETTERS[] = {
            &GLYPH_A, &GLYPH_B, &GLYPH_C, &GLYPH_D, &GLYPH_E,
            &GLYPH_F, &GLYPH_G, &GLYPH_H, &GLYPH_I, &GLYPH_J,
            &GLYPH_K, &GLYPH_L, &GLYPH_M, &GLYPH_N, &GLYPH_O,
            &GLYPH_P, &GLYPH_Q, &GLYPH_R, &GLYPH_S, &GLYPH_T,
            &GLYPH_U, &GLYPH_V, &GLYPH_W, &GLYPH_X, &GLYPH_Y, &GLYPH_Z
        };
        return LETTERS[c - 'A'];
    }

    if (c >= 'a' && c <= 'z') {
        static const Glyph* LOWERCASE[] = {
            &GLYPH_a, &GLYPH_b, &GLYPH_c, &GLYPH_d, &GLYPH_e,
            &GLYPH_f, &GLYPH_g, &GLYPH_h, &GLYPH_i, &GLYPH_j,
            &GLYPH_k, &GLYPH_l, &GLYPH_m, &GLYPH_n, &GLYPH_o,
            &GLYPH_p, &GLYPH_q, &GLYPH_r, &GLYPH_s, &GLYPH_t,
            &GLYPH_u, &GLYPH_v, &GLYPH_w, &GLYPH_x, &GLYPH_y, &GLYPH_z
        };
        return LOWERCASE[c - 'a'];
    }

    // Handle punctuation and special characters
    // Degree symbol: use '\xB0' (176) or define DEGREE_CHAR constant
    switch (static_cast<unsigned char>(c)) {
        case ':': return &GLYPH_COLON;
        case '-': return &GLYPH_MINUS;
        case '.': return &GLYPH_PERIOD;
        case '/': return &GLYPH_SLASH;
        case '%': return &GLYPH_PERCENT;
        case 0xB0: return &GLYPH_DEGREE;  // Degree symbol (Latin-1: 176)
        default: return nullptr;
    }
}

float getCharWidthMultiplier(char c) {
    switch (static_cast<unsigned char>(c)) {
        // Punctuation
        case ':': return 0.5f;
        case '.': return 0.33f;
        case '-': return 0.67f;
        case '/': return 0.5f;
        case ' ': return 0.5f;
        case 0xB0: return 0.33f;  // Degree symbol

        // Narrow lowercase
        case 'i': return 0.4f;
        case 'j': return 0.4f;
        case 'l': return 0.35f;
        case 'r': return 0.6f;
        case 't': return 0.5f;
        case 'f': return 0.5f;

        // Wide lowercase
        case 'm': return 1.0f;
        case 'w': return 1.0f;

        default: return 1.0f;
    }
}

static void scalePoint(uint8_t sx, uint8_t sy, int16_t destX, int16_t destY,
                       int16_t width, int16_t height, int16_t& outX, int16_t& outY) {
    outX = destX + (sx * width) / 100;
    outY = destY + (sy * height) / 100;
}

void renderChar(IFramebuffer& fb, char c, int16_t x, int16_t y,
                int16_t width, int16_t height, int16_t strokeWidth, Color color) {
    const Glyph* glyph = getGlyph(c);
    if (!glyph) return;

    for (uint8_t s = 0; s < glyph->strokeCount; s++) {
        const GlyphStroke& stroke = glyph->strokes[s];
        if (stroke.pointCount < 2) continue;

        for (uint8_t i = 0; i < stroke.pointCount - 1; i++) {
            int16_t x0, y0, x1, y1;
            scalePoint(stroke.points[i * 2], stroke.points[i * 2 + 1],
                       x, y, width, height, x0, y0);
            scalePoint(stroke.points[(i + 1) * 2], stroke.points[(i + 1) * 2 + 1],
                       x, y, width, height, x1, y1);
            drawThickLine(fb, x0, y0, x1, y1, strokeWidth, color);
        }
    }
}

void renderString(IFramebuffer& fb, const char* text, int16_t x, int16_t y,
                  int16_t charWidth, int16_t charHeight, int16_t spacing,
                  int16_t strokeWidth, Color color) {
    int16_t currentX = x;

    for (const char* p = text; *p; p++) {
        char c = *p;
        float widthMult = getCharWidthMultiplier(c);
        int16_t actualWidth = static_cast<int16_t>(charWidth * widthMult);

        if (getGlyph(c)) {
            renderChar(fb, c, currentX, y, actualWidth, charHeight, strokeWidth, color);
        }

        currentX += actualWidth + spacing;
    }
}

int16_t getStringWidth(const char* text, int16_t charWidth, int16_t spacing) {
    if (!text || !*text) return 0;

    int16_t totalWidth = 0;
    size_t len = std::strlen(text);

    for (size_t i = 0; i < len; i++) {
        float widthMult = getCharWidthMultiplier(text[i]);
        totalWidth += static_cast<int16_t>(charWidth * widthMult);

        if (i < len - 1) {
            totalWidth += spacing;
        }
    }

    return totalWidth;
}

void renderStringCentered(IFramebuffer& fb, const char* text, int16_t centerX, int16_t y,
                          int16_t charWidth, int16_t charHeight, int16_t spacing,
                          int16_t strokeWidth, Color color) {
    int16_t stringWidth = getStringWidth(text, charWidth, spacing);
    int16_t x = centerX - stringWidth / 2;
    renderString(fb, text, x, y, charWidth, charHeight, spacing, strokeWidth, color);
}

void renderStringRight(IFramebuffer& fb, const char* text, int16_t rightX, int16_t y,
                       int16_t charWidth, int16_t charHeight, int16_t spacing,
                       int16_t strokeWidth, Color color) {
    int16_t stringWidth = getStringWidth(text, charWidth, spacing);
    int16_t x = rightX - stringWidth;
    renderString(fb, text, x, y, charWidth, charHeight, spacing, strokeWidth, color);
}

void renderMultiline(IFramebuffer& fb, const char* const* lines, size_t lineCount,
                     int16_t x, int16_t y, int16_t charWidth, int16_t charHeight,
                     int16_t lineSpacing, TextAlign align,
                     int16_t charSpacing, int16_t strokeWidth, Color color) {
    int16_t currentY = y;

    for (size_t i = 0; i < lineCount; i++) {
        switch (align) {
            case TextAlign::Center:
                renderStringCentered(fb, lines[i], x, currentY, charWidth, charHeight,
                                     charSpacing, strokeWidth, color);
                break;
            case TextAlign::Right:
                renderStringRight(fb, lines[i], x, currentY, charWidth, charHeight,
                                  charSpacing, strokeWidth, color);
                break;
            default:
                renderString(fb, lines[i], x, currentY, charWidth, charHeight,
                             charSpacing, strokeWidth, color);
                break;
        }

        currentY += charHeight + lineSpacing;
    }
}

} // namespace rendering
