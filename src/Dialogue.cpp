#include "Dialogue.hpp"

#include <algorithm>

namespace sv {

const DialogueNode* findDialogueNode(const DialogueDefinition& dialogue, const std::string& nodeId) {
    const auto found = std::find_if(dialogue.nodes.begin(), dialogue.nodes.end(), [&](const auto& node) {
        return node.id == nodeId;
    });
    return found == dialogue.nodes.end() ? nullptr : &*found;
}

bool DialogueSession::begin(const DialogueDefinition& dialogue) {
    const auto* start = findDialogueNode(dialogue, dialogue.startNodeId);
    if (start == nullptr) return false;
    dialogue_ = &dialogue;
    node_ = start;
    return true;
}

void DialogueSession::end() {
    dialogue_ = nullptr;
    node_ = nullptr;
}

std::vector<const DialogueChoice*> DialogueSession::visibleChoices(const StoryState& story) const {
    std::vector<const DialogueChoice*> visible;
    if (!active()) return visible;
    for (const auto& choice : node_->choices) {
        if (choice.requiredFlag.empty() || story.value(choice.requiredFlag) == choice.requiredFlagValue)
            visible.push_back(&choice);
    }
    return visible;
}

bool DialogueSession::choose(std::size_t visibleChoiceIndex, StoryState& story) {
    const auto visible = visibleChoices(story);
    if (visibleChoiceIndex >= visible.size()) return false;
    const auto& choice = *visible[visibleChoiceIndex];
    const auto* next = choice.nextNodeId.empty() ? nullptr : findDialogueNode(*dialogue_, choice.nextNodeId);
    if (!choice.nextNodeId.empty() && next == nullptr) return false;
    if (!choice.setFlag.empty() && !story.set(choice.setFlag, choice.setFlagValue)) return false;
    if (choice.nextNodeId.empty()) {
        end();
        return true;
    }
    node_ = next;
    return true;
}

} // namespace sv
