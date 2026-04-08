/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * vt_enum_utils.cc - Implementation of enum utilities
 */

#include "vt_enum_utils.hh"
#include "main/business/sales.hh"  // For SalesGroupType

namespace vt {

// Sales group display names (simplified for now - can be enhanced with translation later)
std::string GetSalesGroupDisplayName(SalesGroupType group) {
    switch (group) {
        case SalesGroupUnused:      return "Unused";
        case SalesGroupFood:        return "Food";
        case SalesGroupBeverage:    return "Beverage";
        case SalesGroupBeer:        return "Beer";
        case SalesGroupWine:        return "Wine";
        case SalesGroupAlcohol:     return "Alcohol";
        case SalesGroupMerchandise: return "Merchandise";
        case SalesGroupRoom:        return "Room";
        default:                    return "Unknown";
    }
}

std::string GetSalesGroupShortName(SalesGroupType group) {
    switch (group) {
        case SalesGroupUnused:      return "";
        case SalesGroupFood:        return "Food";
        case SalesGroupBeverage:    return "Bev";
        case SalesGroupBeer:        return "Beer";
        case SalesGroupWine:        return "Wine";
        case SalesGroupAlcohol:     return "Alcohol";
        case SalesGroupMerchandise: return "Merchan";
        case SalesGroupRoom:        return "Room";
        default:                    return "";
    }
}

} // namespace vt
