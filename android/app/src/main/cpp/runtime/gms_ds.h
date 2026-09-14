#pragma once

#include "gms_types.h"
#include <unordered_map>
#include <vector>
#include <map>

class GMS_DataStructures {
public:
    static GMS_DataStructures& get() {
        static GMS_DataStructures instance;
        return instance;
    }

    // DS MAP
    int ds_map_create() {
        int id = m_nextMapId++;
        m_maps[id] = std::unordered_map<std::string, GMS_Value>();
        return id;
    }

    void ds_map_destroy(int id) {
        m_maps.erase(id);
    }

    void ds_map_clear(int id) {
        if (m_maps.find(id) != m_maps.end()) {
            m_maps[id].clear();
        }
    }

    bool ds_map_exists(int id, const std::string& key) {
        if (m_maps.find(id) == m_maps.end()) return false;
        return m_maps[id].find(key) != m_maps[id].end();
    }

    void ds_map_add(int id, const std::string& key, const GMS_Value& val) {
        if (m_maps.find(id) == m_maps.end()) {
            m_maps[id] = std::unordered_map<std::string, GMS_Value>();
        }
        m_maps[id][key] = val;
    }

    void ds_map_set(int id, const std::string& key, const GMS_Value& val) {
        ds_map_add(id, key, val);
    }

    GMS_Value ds_map_find_value(int id, const std::string& key) {
        if (m_maps.find(id) != m_maps.end()) {
            auto it = m_maps[id].find(key);
            if (it != m_maps[id].end()) return it->second;
        }
        return GMS_Value(); // Undefined
    }

    int ds_map_size(int id) {
        if (m_maps.find(id) != m_maps.end()) return static_cast<int>(m_maps[id].size());
        return 0;
    }

    // DS LIST
    int ds_list_create() {
        int id = m_nextListId++;
        m_lists[id] = std::vector<GMS_Value>();
        return id;
    }

    void ds_list_destroy(int id) {
        m_lists.erase(id);
    }

    void ds_list_clear(int id) {
        if (m_lists.find(id) != m_lists.end()) {
            m_lists[id].clear();
        }
    }

    void ds_list_add(int id, const GMS_Value& val) {
        if (m_lists.find(id) == m_lists.end()) {
            m_lists[id] = std::vector<GMS_Value>();
        }
        m_lists[id].push_back(val);
    }

    GMS_Value ds_list_find_value(int id, int pos) {
        if (m_lists.find(id) != m_lists.end() && pos >= 0 && pos < static_cast<int>(m_lists[id].size())) {
            return m_lists[id][pos];
        }
        return GMS_Value();
    }

    void ds_list_set(int id, int pos, const GMS_Value& val) {
        if (m_lists.find(id) != m_lists.end() && pos >= 0) {
            if (pos >= static_cast<int>(m_lists[id].size())) {
                m_lists[id].resize(pos + 1);
            }
            m_lists[id][pos] = val;
        }
    }

    int ds_list_size(int id) {
        if (m_lists.find(id) != m_lists.end()) return static_cast<int>(m_lists[id].size());
        return 0;
    }

    void ds_list_delete(int id, int pos) {
        if (m_lists.find(id) != m_lists.end() && pos >= 0 && pos < static_cast<int>(m_lists[id].size())) {
            m_lists[id].erase(m_lists[id].begin() + pos);
        }
    }

    // DS GRID
    int ds_grid_create(int w, int h) {
        int id = m_nextGridId++;
        m_grids[id] = GridData{ w, h, std::vector<GMS_Value>(w * h, GMS_Value(0.0)) };
        return id;
    }

    void ds_grid_destroy(int id) {
        m_grids.erase(id);
    }

    void ds_grid_set(int id, int x, int y, const GMS_Value& val) {
        if (m_grids.find(id) != m_grids.end()) {
            auto& g = m_grids[id];
            if (x >= 0 && x < g.w && y >= 0 && y < g.h) {
                g.data[y * g.w + x] = val;
            }
        }
    }

    GMS_Value ds_grid_get(int id, int x, int y) {
        if (m_grids.find(id) != m_grids.end()) {
            auto& g = m_grids[id];
            if (x >= 0 && x < g.w && y >= 0 && y < g.h) {
                return g.data[y * g.w + x];
            }
        }
        return GMS_Value(0.0);
    }

    void ds_grid_clear(int id, const GMS_Value& val) {
        if (m_grids.find(id) != m_grids.end()) {
            auto& g = m_grids[id];
            std::fill(g.data.begin(), g.data.end(), val);
        }
    }

private:
    struct GridData {
        int w = 0;
        int h = 0;
        std::vector<GMS_Value> data;
    };

    int m_nextMapId = 1;
    int m_nextListId = 1;
    int m_nextGridId = 1;

    std::unordered_map<int, std::unordered_map<std::string, GMS_Value>> m_maps;
    std::unordered_map<int, std::vector<GMS_Value>> m_lists;
    std::unordered_map<int, GridData> m_grids;
};
