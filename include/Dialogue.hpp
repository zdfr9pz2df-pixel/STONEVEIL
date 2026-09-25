#pragma once

#include "StoryState.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sv {

struct DialogueChoice {
    std::string id;
    std::string text;
    std::string nextNodeId;
    std::string requiredFlag;
    bool requiredFlagValue{true};
    std::string setFlag;
    bool setFlagValue{true};
};

struct DialogueNode {
    std::string id;
    std::string speaker;
    std::string text;
    std::vector<DialogueChoice> choices;
};

struct DialogueDefinition {
    std::string id;
    std::string name;
    std::string startNodeId;
    std::vector<DialogueNode> nodes;
};

const DialogueNode* findDialogueNode(const DialogueDefinition& dialogue, const std::string& nodeId);

// A small, presentation-free conversation state machine. Story flags are the
// durable output, so an in-progress modal never needs to enter the save file.
class DialogueSession {
public:
    bool begin(const DialogueDefinition& dialogue);
    void end();
    bool active() const { return dialogue_ != nullptr && node_ != nullptr; }
    const DialogueDefinition* dialogue() const { return dialogue_; }
    const DialogueNode* node() const { return node_; }
    std::vector<const DialogueChoice*> visibleChoices(const StoryState& story) const;
    bool choose(std::size_t visibleChoiceIndex, StoryState& story);

private:
    const DialogueDefinition* dialogue_{nullptr};
    const DialogueNode* node_{nullptr};
};

} // namespace sv
