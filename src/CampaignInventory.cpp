#include "CampaignInventory.hpp"

#include <algorithm>

namespace sv {

void CampaignInventory::reset() {
    keys_ = 0;
    potions_ = 1;
    items_.clear();
}

void CampaignInventory::addKeys(int count) {
    if (count > 0) keys_ += count;
}

bool CampaignInventory::consumeKey() {
    if (keys_ <= 0) return false;
    --keys_;
    return true;
}

void CampaignInventory::addPotions(int count) {
    if (count > 0) potions_ += count;
}

bool CampaignInventory::consumePotion() {
    if (potions_ <= 0) return false;
    --potions_;
    return true;
}

bool CampaignInventory::hasItem(const std::string& itemId) const {
    return std::find(items_.begin(), items_.end(), itemId) != items_.end();
}

bool CampaignInventory::addItem(const std::string& itemId) {
    if (itemId.empty() || hasItem(itemId)) return false;
    items_.push_back(itemId);
    return true;
}

bool CampaignInventory::restore(int keys, int potions, const std::vector<std::string>& items) {
    if (keys < 0 || potions < 0) return false;
    for (const auto& item : items) {
        if (item.empty()) return false;
    }
    keys_ = keys;
    potions_ = potions;
    items_ = items;
    std::sort(items_.begin(), items_.end());
    items_.erase(std::unique(items_.begin(), items_.end()), items_.end());
    return true;
}

} // namespace sv
