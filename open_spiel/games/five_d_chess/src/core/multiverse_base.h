//
//  board_2d.h
//  5dchess_engine
//
//  Created by ftxi on 2024/12/5.
//

#ifndef MULTIVERSE_BASE_H
#define MULTIVERSE_BASE_H

#include <string>
#include <vector>
#include <tuple>
#include <utility>
#include <map>
#include <memory>
#include "turn.h"
#include "board.h"
#include "vec4.h"
#include "generator.h"

using movegen_t = generator<std::pair<vec4, bitboard_t>>;
using boards_info_t = std::tuple<int,int,bool,std::string>; // l, t, color, fen

/*
 The multiverse class.

 Behavior of copying a multiverse object is just copy the vector of vectors of pointers to the boards. It does not perform deep-copy of a board object. (Which is expected.)

 This is an abstract base class. Two child classes are defined for odd and even timelines respectively.
 */
class multiverse
{
private:
    const int size_x, size_y; // board size
    std::vector<std::vector<std::shared_ptr<board>>> boards;
    // the following data are derivated from boards:
    int l_min, l_max, active_min, active_max;
    std::vector<int> timeline_start, timeline_end;
    
    // private methods for move generation
    template<piece_t P, bool C>
    bitboard_t gen_physical_moves_impl(vec4 p) const;

    template<piece_t P, bool C, bool ONLY_SP>
    movegen_t gen_moves_impl(vec4 p) const;

    template<bool C>
    generator<vec4> gen_board_move_impl(vec4 p0) const;

    /*
     generate sliding moves in directions that are:
      + starting from `p`
      + in superphysical directions, moves as axesmode `TL`
        - when `TL` is `ORTHOGONAL`, moves to p+(*,*,1,0), p+(*,*,1,0), etc.
        - when `TL` is `DIAGONAL`, moves to p+(*,*,1,1), p+(*,*,1,-1), etc.
        - `BOTH` means both of the above
      + in physical directions, moves as axesmode `XY`
        - when `XY` is `ORTHOGONAL`, moves to p+(1,0,*,*), p+(0,1,*,*), etc.
        - when `XY` is `DIAGONAL`, moves to p+(1,1,*,*), p+(1,-1,*,*), etc.
        - `BOTH` means both of the above
     */
    enum class axesmode {ORTHOGONAL, DIAGONAL, BOTH};
    template<bool C, axesmode TL, axesmode XY>
    void gen_compound_moves(vec4 p, std::map<vec4, bitboard_t>& result) const;

    template<bool C>
    std::vector<std::pair<vec4, bitboard_t>> gen_purely_sp_rook_moves(vec4 p0) const;
    
    template<bool C>
    std::vector<std::pair<vec4, bitboard_t>> gen_purely_sp_bishop_moves(vec4 p0) const;
    
    template<bool C>
    std::vector<std::pair<vec4, bitboard_t>> gen_purely_sp_knight_moves(vec4 p0) const;

    void insert_board_impl(int l, int t, bool c, const std::shared_ptr<board>& b_ptr);
protected:
    virtual std::pair<int,int> calculate_active_range() const = 0;
    void update_active_range(); // for initialization of derived classes only
public:
    // constructor
    multiverse(std::vector<boards_info_t> boards, int size_x, int size_y);
    
    // modifiers
    void insert_board(int l, int t, bool c, const std::shared_ptr<board>& b_ptr);
    void append_board(int l, const std::shared_ptr<board>& b_ptr);
	void drop_board(int l);

    // getters
    std::pair<int, int> get_board_size() const;
    virtual std::pair<int, int> get_initial_lines_range() const = 0;
    std::pair<int, int> get_lines_range() const;
    std::pair<int, int> get_active_range() const;
    turn_t get_timeline_start(int l) const;
    turn_t get_timeline_end(int l) const;
    
    std::shared_ptr<board> get_board(int l, int t, bool c) const;
    
    template<bool SHOW_UMOVE=false>
    std::vector<boards_info_t> get_boards() const;
	void get_one_board_and_edge(int u, int v, std::vector<std::pair<int,std::vector<uint64_t>>>& all_boards, std::vector<std::pair<int,int>>& boards_edges) const;
	std::tuple<std::vector<std::pair<int,std::vector<uint64_t>>>, std::vector<std::pair<int,int>>> get_boards_and_edges() const;
	template <bool COLOR> std::vector<std::pair<int,std::vector<uint64_t>>> get_operable_boards_moves(bool allow_pass = false) const;
    
    std::string to_string() const;
    piece_t get_piece(vec4 a, bool color) const;
    bool get_umove_flag(vec4 a, bool color) const;
    
    /*
     This helper function returns (present_t, present_c)
     where: present_t is the time of present in L,T coordinate
            present_c is either false (for white) or true (for black)
     */
    turn_t get_present() const;
    
    // move generation
    template<bool C> bitboard_t gen_physical_moves(vec4 p) const;
    template<bool C> movegen_t gen_superphysical_moves(vec4 p) const;
    template<bool C> movegen_t gen_moves(vec4 p) const;
    generator<vec4> gen_piece_move(vec4 p, bool board_color) const;
    
    // help functions
    bool inbound(vec4 a, bool color) const;
	bool outofrange(vec4 p, vec4 q, bool color) const;
    virtual std::unique_ptr<multiverse> clone() const = 0;
    virtual std::string pretty_l(int l) const = 0;
    virtual std::string pretty_lt(vec4 p0) const = 0;
    virtual ~multiverse() = default;
};

/*
 The following static functions describe the correspondence between two coordinate systems: L,T and u,v
 
l_to_u make use of the bijection from integers to non-negative integers:
x -> ~(x>>1)
 */
constexpr static int l_to_u(int l)
{
    if(l >= 0)
        return l << 1;
    else
        return ~(l << 1);
}

constexpr static int tc_to_v(int t, bool c)
{
    return t << 1 | static_cast<int>(c);
}

constexpr static int u_to_l(int u)
{
    if(u & 1)
        return ~(u >> 1);
    else
        return u >> 1;
}

constexpr static std::pair<int, bool> v_to_tc(int v)
{
    return {v >> 1, static_cast<bool>(v & 1)};
}

/**
 * @brief Encodes 5D Chess move parameters into a single 64-bit unique move ID
 * @note Flags field expanded from 4 bits to 8 bits (0-255 value range)
 * @param u0 Source timeline layer (U axis)
 * @param v0 Source timeline branch (V axis)
 * @param y0 Source board Y coordinate
 * @param x0 Source board X coordinate
 * @param u1 Destination timeline layer (U axis)
 * @param v1 Destination timeline branch (V axis)
 * @param y1 Destination board Y coordinate
 * @param x1 Destination board X coordinate
 * @param pto Piece promotion type (4 bits)
 * @param flags Move status flags (8 bits, expanded)
 * @return Encoded 64-bit move identifier
 * 
 * Bit-field Layout (Total 56 bits used, 8 bits reserved):
 * [Flags(8bit:0-7) | Pto(4bit:8-11) | X1(3bit:12-14) | Y1(3bit:15-17) | V1(8bit:18-25) | U1(8bit:26-33)]
 * [X0(3bit:34-36) | Y0(3bit:37-39) | V0(8bit:40-47) | U0(8bit:48-55)]
 */
constexpr static uint64_t EncodeMoveId(int u0, int v0, int y0, int x0,
                                     int u1, int v1, int y1, int x1,
                                     int pto, int flags) {
  uint64_t moveid = 0;
  
  // Move status flags: 8-bit width
  moveid |= (static_cast<uint64_t>(flags) & 0xFFULL) << 0;
  // Piece promotion type: 4-bit width
  moveid |= (static_cast<uint64_t>(pto) & 0xFULL) << 8;
  
  // Destination position coordinates
  moveid |= (static_cast<uint64_t>(x1) & 0x7ULL) << 12;
  moveid |= (static_cast<uint64_t>(y1) & 0x7ULL) << 15;
  moveid |= (static_cast<uint64_t>(v1) & 0xFFULL) << 18;
  moveid |= (static_cast<uint64_t>(u1) & 0xFFULL) << 26;
  
  // Source position coordinates
  moveid |= (static_cast<uint64_t>(x0) & 0x7ULL) << 34;
  moveid |= (static_cast<uint64_t>(y0) & 0x7ULL) << 37;
  moveid |= (static_cast<uint64_t>(v0) & 0xFFULL) << 40;
  moveid |= (static_cast<uint64_t>(u0) & 0xFFULL) << 48;
  
  return moveid;
}

/**
 * @brief Decodes a 64-bit move ID back to original 5D Chess move parameters
 * @param moveid Encoded 64-bit move identifier
 * @return Tuple of decoded parameters: (u0, v0, y0, x0, u1, v1, y1, x1, pto, flags)
 */
constexpr static std::tuple<int, int, int, int, int, int, int, int, int, int> 
DecodeMoveId(uint64_t moveid) {
  // Decode source position coordinates
  int u0 = static_cast<int>((moveid >> 48) & 0xFF);
  int v0 = static_cast<int>((moveid >> 40) & 0xFF);
  int y0 = static_cast<int>((moveid >> 37) & 0x7);
  int x0 = static_cast<int>((moveid >> 34) & 0x7);
  
  // Decode destination position coordinates
  int u1 = static_cast<int>((moveid >> 26) & 0xFF);
  int v1 = static_cast<int>((moveid >> 18) & 0xFF);
  int y1 = static_cast<int>((moveid >> 15) & 0x7);
  int x1 = static_cast<int>((moveid >> 12) & 0x7);
  
  // Decode piece promotion type and move status flags
  int pto = static_cast<int>((moveid >> 8) & 0xF);
  int flags = static_cast<int>((moveid >> 0) & 0xFF);
  
  return {u0, v0, y0, x0, u1, v1, y1, x1, pto, flags};
}

constexpr static int EncodeBoardId(int u, int v) {
  return static_cast<int>(((u & 0xFF) << 8) | (v & 0xFF));
}

constexpr static std::pair<int, int> DecodeBoardId(int board_id) {
  int u = (board_id >> 8) & 0xFF;
  int v = board_id & 0xFF;
  return {u, v};
}

#endif /* MULTIVERSE_BASE_H */
