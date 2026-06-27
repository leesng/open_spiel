#pragma once
#include <iostream>
#include <future>
#include "assets/bishop-black.svg.hpp"
#include "assets/bishop-white.svg.hpp"
#include "assets/brawn-black.svg.hpp"
#include "assets/brawn-white.svg.hpp"
#include "assets/commonking-black.svg.hpp"
#include "assets/commonking-white.svg.hpp"
#include "assets/dragon-black.svg.hpp"
#include "assets/dragon-white.svg.hpp"
#include "assets/king-black.svg.hpp"
#include "assets/king-white.svg.hpp"
#include "assets/knight-black.svg.hpp"
#include "assets/knight-white.svg.hpp"
#include "assets/pawn-black.svg.hpp"
#include "assets/pawn-white.svg.hpp"
#include "assets/princess-black.svg.hpp"
#include "assets/princess-white.svg.hpp"
#include "assets/queen-black.svg.hpp"
#include "assets/queen-white.svg.hpp"
#include "assets/rook-black.svg.hpp"
#include "assets/rook-white.svg.hpp"
#include "assets/royalqueen-black.svg.hpp"
#include "assets/royalqueen-white.svg.hpp"
#include "assets/unicorn-black.svg.hpp"
#include "assets/unicorn-white.svg.hpp"

#include "assets/seguiemj_font.hpp"

// ===================== 5D Chess Piece Texture LOD Manager =====================
class FiveDChessTexture
{
    // Delete constructor (singleton pattern)
    FiveDChessTexture() = delete;

    inline static const std::vector<int> LOD_SIZES = {128, 64, 32, 16, 8, 4, 2};
    inline static const std::unordered_map<char, std::pair<std::string, std::string>> PIECE_SVG_MAPPING = {
        {'b', {"assets/bishop-black.svg",        bishop_black_svg}},
        {'B', {"assets/bishop-white.svg",        bishop_white_svg}},
        {'w', {"assets/brawn-black.svg",         brawn_black_svg}},
        {'W', {"assets/brawn-white.svg",         brawn_white_svg}},
        {'c', {"assets/commonking-black.svg",    commonking_black_svg}},
        {'C', {"assets/commonking-white.svg",    commonking_white_svg}},
        {'d', {"assets/dragon-black.svg",        dragon_black_svg}},
        {'D', {"assets/dragon-white.svg",        dragon_white_svg}},
        {'k', {"assets/king-black.svg",          king_black_svg}},
        {'K', {"assets/king-white.svg",          king_white_svg}},
        {'n', {"assets/knight-black.svg",        knight_black_svg}},
        {'N', {"assets/knight-white.svg",        knight_white_svg}},
        {'p', {"assets/pawn-black.svg",          pawn_black_svg}},
        {'P', {"assets/pawn-white.svg",          pawn_white_svg}},
        {'s', {"assets/princess-black.svg",      princess_black_svg}},
        {'S', {"assets/princess-white.svg",      princess_white_svg}},
        {'q', {"assets/queen-black.svg",         queen_black_svg}},
        {'Q', {"assets/queen-white.svg",         queen_white_svg}},
        {'r', {"assets/rook-black.svg",          rook_black_svg}},
        {'R', {"assets/rook-white.svg",          rook_white_svg}},
        {'y', {"assets/royalqueen-black.svg",    royalqueen_black_svg}},
        {'Y', {"assets/royalqueen-white.svg",    royalqueen_white_svg}},
        {'u', {"assets/unicorn-black.svg",       unicorn_black_svg}},
        {'U', {"assets/unicorn-white.svg",       unicorn_white_svg}}
    };

    inline static std::atomic<bool> m_isLoaded = false; // Atomic load flag (thread-safe)
    inline static std::unordered_map<int, std::unordered_map<char, sf::Texture>> m_lodTextures; // Global LOD textures: Size -> PieceChar -> sf::Texture

    // Render SVG to specified size texture
    static sf::Texture loadSVGFromPath(const std::string& svgPath, int targetSize); 
    static sf::Texture loadSVGFromStr(const std::string& svgContent, int targetSize);

public:
    // Async load all LOD textures (execute only once)
    static void loadAll() {

        if (m_isLoaded.load(std::memory_order_acquire)) {
            std::cout << TS << "Textures loaded, skipped!" << std::endl;
            return;
        }

        for (int size : LOD_SIZES) {
            m_lodTextures[size] = std::unordered_map<char, sf::Texture>();
        }

        std::vector<std::future<void>> tasks;
        for (auto& [piece, svgPair] : PIECE_SVG_MAPPING) {
            const std::string& svgPath = svgPair.first;
            const std::string& svgContent = svgPair.second;

            tasks.emplace_back(std::async(std::launch::async, [piece, svgPath, svgContent]() {
                try {
                    for (int size : LOD_SIZES) {
                        sf::Texture lodTex;

                        if (std::filesystem::exists(svgPath) && std::filesystem::is_regular_file(svgPath)) {

                            lodTex = loadSVGFromPath(svgPath, size);
                        } else {
                            lodTex = loadSVGFromStr(svgContent, size);
                        }

                        m_lodTextures[size][piece] = std::move(lodTex);
                    }
                }
                catch (const std::exception& e) {
                    std::cout << TS << "Load piece '" << piece << "' error: " << e.what() << std::endl;
                }
            }));
        }

        for (auto& task : tasks) {
            task.get();
        }
		
		m_font = sf::Font(seguiemj_ttf, seguiemj_ttf_len);

        std::cout << TS << "All resource loaded!" << std::endl;
        m_isLoaded.store(true, std::memory_order_release);
    }

    // Choose LOD texture set by actual pixel size
    static const std::unordered_map<char, sf::Texture>& chooseLOD(float pixelSize)  {
        if (pixelSize >= 60)  return m_lodTextures.at(128);
        if (pixelSize >= 30)  return m_lodTextures.at(64);
        if (pixelSize >= 15)  return m_lodTextures.at(32);
        if (pixelSize >= 8)   return m_lodTextures.at(16);
        if (pixelSize >= 4)   return m_lodTextures.at(8);
        if (pixelSize >= 2)   return m_lodTextures.at(4);
        return m_lodTextures.at(2);
    }
	
	inline static sf::Font m_font;
};
