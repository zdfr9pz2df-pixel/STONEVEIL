#pragma once

#include <string>
#include <vector>

namespace sv {

// Campaign inventory is PARTY state, never character state.
//
// The rule this type exists to enforce: a progression-critical object cannot be
// lost because the character carrying it died. Nothing here is owned by a
// CharacterId, so there is no carrier to kill. Personal equipment, when it
// exists, will live on CharacterRecord instead and may follow a different
// recovery rule.
//
// Keys are counted rather than itemised because the prototype only needs "can
// this door open"; named progression items are tracked individually because
// their identity matters.
class CampaignInventory {
public:
    void reset();

    int keys() const { return keys_; }
    void addKeys(int count);
    bool consumeKey();

    int potions() const { return potions_; }
    void addPotions(int count);
    bool consumePotion();

    bool hasItem(const std::string& itemId) const;
    bool addItem(const std::string& itemId);   // false when already held
    const std::vector<std::string>& items() const { return items_; }

    // Restoration path for save loading; rejects negative counts.
    bool restore(int keys, int potions, const std::vector<std::string>& items);

private:
    int keys_{0};
    int potions_{1};
    std::vector<std::string> items_;
};

} // namespace sv
