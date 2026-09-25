#include "Dialogue.hpp"
#include "Dungeon.hpp"
#include "LevelIO.hpp"
#include "WorldEvents.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)

using namespace sv;

namespace {

DialogueDefinition testDialogue() {
    DialogueDefinition dialogue;
    dialogue.id = "dialogue.test.guard";
    dialogue.name = "Guard test";
    dialogue.startNodeId = "greeting";
    DialogueNode greeting{"greeting", "Guard", "State your business.", {}};
    greeting.choices.push_back({"ask", "Ask about the gate.", "answer", {}, true,
                                "test.asked-about-gate", true});
    greeting.choices.push_back({"secret", "Use the password.", "answer",
                                "test.password-known", true, "test.guard-friendly", true});
    greeting.choices.push_back({"deny", "Deny knowing the password.", {},
                                "test.password-known", false, "test.guard-friendly", false});
    DialogueNode answer{"answer", "Guard", "Then you know enough.", {}};
    answer.choices.push_back({"leave", "Leave.", {}, {}, true, {}, true});
    dialogue.nodes = {greeting, answer};
    return dialogue;
}

LevelDefinition testLevel() {
    LevelDefinition level;
    level.id = "dialogue.test.level";
    level.name = "Dialogue Test";
    level.width = 4;
    level.height = 4;
    level.map = {"####", "#..#", "#.E#", "####"};
    level.spawnX = 1;
    level.spawnY = 1;
    WorldObject npc;
    npc.id = "npc.test.guard";
    npc.kind = WorldObjectKind::Npc;
    npc.x = 1;
    npc.y = 2;
    npc.name = "Test Guard";
    npc.dialogueId = "dialogue.test.guard";
    level.objects.push_back(npc);
    level.dialogues.push_back(testDialogue());
    return level;
}

} // namespace

int main() {
    StoryState story;
    const auto dialogue = testDialogue();
    DialogueSession session;
    CHECK(session.begin(dialogue));
    CHECK(session.node()->id == "greeting");
    auto choices = session.visibleChoices(story);
    CHECK(choices.size() == 2);
    CHECK(choices[0]->id == "ask" && choices[1]->id == "deny");
    CHECK(session.choose(0, story));
    CHECK(story.value("test.asked-about-gate"));
    CHECK(session.node()->id == "answer");
    CHECK(session.choose(0, story));
    CHECK(!session.active());
    CHECK(!session.choose(0, story));

    CHECK(story.set("test.password-known"));
    CHECK(session.begin(dialogue));
    choices = session.visibleChoices(story);
    CHECK(choices.size() == 2);
    CHECK(choices[1]->id == "secret");
    CHECK(session.choose(1, story));
    CHECK(story.value("test.guard-friendly"));

    auto level = testLevel();
    CHECK(LevelIO::validate(level).empty());
    const char* path = "dialogue-roundtrip.svl";
    std::string error;
    CHECK(LevelIO::save(path, level, error));
    LevelDefinition loaded;
    CHECK(LevelIO::load(path, loaded, error));
    CHECK(loaded.dialogues.size() == 1);
    CHECK(loaded.dialogues[0].nodes.size() == 2);
    CHECK(loaded.dialogues[0].nodes[0].choices[1].requiredFlag == "test.password-known");
    CHECK(loaded.objects[0].dialogueId == "dialogue.test.guard");
    std::remove(path);

    Dungeon dungeon{loaded};
    EventRuntime events;
    CHECK(configureWorldEvents(dungeon, events));
    std::string started;
    WorldEventPresentation presentation;
    presentation.startDialogue = [&](const std::string& id) { started = id; return true; };
    int keys{};
    const auto fired = dispatchWorldEvent(events,
        {EventTriggerType::InteractObject, 1, 2, "npc.test.guard"}, dungeon, keys, presentation, &story);
    CHECK(fired.eventsRun == 1 && fired.consumed);
    CHECK(started == "dialogue.test.guard");

    auto broken = testLevel();
    broken.dialogues[0].nodes[0].choices[0].nextNodeId = "missing";
    CHECK(!LevelIO::validate(broken).empty());
    broken = testLevel();
    broken.dialogues[0].nodes.push_back({"unused", "Nobody", "Unreachable.",
                                        {{"end", "End.", {}, {}, true, {}, true}}});
    CHECK(!LevelIO::validate(broken).empty());
    broken = testLevel();
    broken.objects[0].dialogueId = "dialogue.missing";
    CHECK(!LevelIO::validate(broken).empty());
    return 0;
}
