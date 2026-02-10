#include <gtest/gtest.h>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/quest_components.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/quest_types.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// =============================================================================
// QuestObjective component tests (SRS-GML-005.2)
// =============================================================================

TEST(QuestObjectiveTest, DefaultState) {
    QuestObjective obj;
    EXPECT_EQ(obj.type, ObjectiveType::Kill);
    EXPECT_EQ(obj.current, 0);
    EXPECT_EQ(obj.required, 1);
    EXPECT_FALSE(obj.completed);
    EXPECT_FALSE(obj.IsComplete());
}

TEST(QuestObjectiveTest, AddProgressUpdates) {
    QuestObjective obj;
    obj.required = 5;

    obj.AddProgress(2);
    EXPECT_EQ(obj.current, 2);
    EXPECT_FALSE(obj.IsComplete());

    obj.AddProgress(3);
    EXPECT_EQ(obj.current, 5);
    EXPECT_TRUE(obj.IsComplete());
    EXPECT_TRUE(obj.completed);
}

TEST(QuestObjectiveTest, ProgressClampsToRequired) {
    QuestObjective obj;
    obj.required = 3;

    obj.AddProgress(10);
    EXPECT_EQ(obj.current, 3);
    EXPECT_TRUE(obj.IsComplete());
}

TEST(QuestObjectiveTest, AddProgressAfterCompletionIsNoop) {
    QuestObjective obj;
    obj.required = 2;
    obj.AddProgress(2);
    EXPECT_TRUE(obj.completed);

    obj.AddProgress(5);
    EXPECT_EQ(obj.current, 2);  // Unchanged.
}

// =============================================================================
// QuestEntry tests
// =============================================================================

TEST(QuestEntryTest, AllObjectivesComplete) {
    QuestEntry entry;
    entry.questId = 1;
    entry.state = QuestState::Accepted;

    QuestObjective obj1;
    obj1.type = ObjectiveType::Kill;
    obj1.targetId = 100;
    obj1.required = 3;

    QuestObjective obj2;
    obj2.type = ObjectiveType::Collect;
    obj2.targetId = 200;
    obj2.required = 5;

    entry.objectives = {obj1, obj2};

    EXPECT_FALSE(entry.AllObjectivesComplete());

    entry.objectives[0].AddProgress(3);
    EXPECT_FALSE(entry.AllObjectivesComplete());

    entry.objectives[1].AddProgress(5);
    EXPECT_TRUE(entry.AllObjectivesComplete());
}

TEST(QuestEntryTest, UpdateObjectiveMatchesTypeAndTarget) {
    QuestEntry entry;
    entry.questId = 1;
    entry.state = QuestState::Accepted;

    QuestObjective killObj;
    killObj.type = ObjectiveType::Kill;
    killObj.targetId = 100;
    killObj.required = 3;

    QuestObjective collectObj;
    collectObj.type = ObjectiveType::Collect;
    collectObj.targetId = 200;
    collectObj.required = 2;

    entry.objectives = {killObj, collectObj};

    // Kill event for creature 100.
    EXPECT_TRUE(entry.UpdateObjective(ObjectiveType::Kill, 100));
    EXPECT_EQ(entry.objectives[0].current, 1);
    EXPECT_EQ(entry.objectives[1].current, 0);  // Unaffected.

    // Kill event for wrong creature.
    EXPECT_FALSE(entry.UpdateObjective(ObjectiveType::Kill, 999));
    EXPECT_EQ(entry.objectives[0].current, 1);
}

TEST(QuestEntryTest, UpdateObjectiveTransitionsToComplete) {
    QuestEntry entry;
    entry.questId = 1;
    entry.state = QuestState::Accepted;

    QuestObjective obj;
    obj.type = ObjectiveType::Kill;
    obj.targetId = 100;
    obj.required = 1;

    entry.objectives = {obj};

    entry.UpdateObjective(ObjectiveType::Kill, 100);
    EXPECT_EQ(entry.state, QuestState::ObjectivesComplete);
}

TEST(QuestEntryTest, UpdateObjectiveIgnoresNonAcceptedState) {
    QuestEntry entry;
    entry.questId = 1;
    entry.state = QuestState::ObjectivesComplete;

    QuestObjective obj;
    obj.type = ObjectiveType::Kill;
    obj.targetId = 100;
    obj.required = 5;

    entry.objectives = {obj};

    EXPECT_FALSE(entry.UpdateObjective(ObjectiveType::Kill, 100));
    EXPECT_EQ(entry.objectives[0].current, 0);
}

// =============================================================================
// QuestLog component tests (SRS-GML-005.1)
// =============================================================================

class QuestLogTest : public ::testing::Test {
protected:
    QuestTemplate makeSimpleTemplate(uint32_t id, uint32_t killTarget = 100,
                                     int32_t killCount = 3) {
        QuestTemplate tmpl;
        tmpl.id = id;
        tmpl.name = "Test Quest " + std::to_string(id);
        tmpl.level = 1;

        QuestObjective obj;
        obj.type = ObjectiveType::Kill;
        obj.targetId = killTarget;
        obj.required = killCount;
        tmpl.objectives = {obj};

        tmpl.rewards.experience = 100;
        tmpl.rewards.currency = 50;
        return tmpl;
    }
};

TEST_F(QuestLogTest, AcceptQuest) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1);

    EXPECT_TRUE(log.Accept(tmpl));
    EXPECT_TRUE(log.HasQuest(1));
    EXPECT_EQ(log.activeQuests.size(), 1u);
    EXPECT_EQ(log.activeQuests[0].state, QuestState::Accepted);
}

TEST_F(QuestLogTest, AcceptDuplicateQuestFails) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1);

    EXPECT_TRUE(log.Accept(tmpl));
    EXPECT_FALSE(log.Accept(tmpl));
    EXPECT_EQ(log.activeQuests.size(), 1u);
}

TEST_F(QuestLogTest, AcceptMaxQuestsReached) {
    QuestLog log;
    log.maxActiveQuests = 2;

    EXPECT_TRUE(log.Accept(makeSimpleTemplate(1)));
    EXPECT_TRUE(log.Accept(makeSimpleTemplate(2)));
    EXPECT_FALSE(log.Accept(makeSimpleTemplate(3)));
    EXPECT_EQ(log.activeQuests.size(), 2u);
}

TEST_F(QuestLogTest, AbandonQuest) {
    QuestLog log;
    log.Accept(makeSimpleTemplate(1));
    log.Accept(makeSimpleTemplate(2));

    EXPECT_TRUE(log.Abandon(1));
    EXPECT_FALSE(log.HasQuest(1));
    EXPECT_TRUE(log.HasQuest(2));
    EXPECT_EQ(log.activeQuests.size(), 1u);
}

TEST_F(QuestLogTest, AbandonNonexistentQuestFails) {
    QuestLog log;
    EXPECT_FALSE(log.Abandon(999));
}

TEST_F(QuestLogTest, TurnInCompletedQuest) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1, 100, 1);
    log.Accept(tmpl);

    // Complete the objective.
    auto* entry = log.GetQuest(1);
    ASSERT_NE(entry, nullptr);
    entry->UpdateObjective(ObjectiveType::Kill, 100);
    EXPECT_EQ(entry->state, QuestState::ObjectivesComplete);

    // Turn in.
    EXPECT_TRUE(log.TurnIn(1));
    EXPECT_EQ(entry->state, QuestState::TurnedIn);
    EXPECT_TRUE(log.IsCompleted(1));
}

TEST_F(QuestLogTest, TurnInIncompleteQuestFails) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1, 100, 5);
    log.Accept(tmpl);

    EXPECT_FALSE(log.TurnIn(1));
}

TEST_F(QuestLogTest, TurnInNonexistentQuestFails) {
    QuestLog log;
    EXPECT_FALSE(log.TurnIn(999));
}

TEST_F(QuestLogTest, GetQuestReturnsNullForMissing) {
    QuestLog log;
    EXPECT_EQ(log.GetQuest(999), nullptr);
}

TEST_F(QuestLogTest, CleanupFinished) {
    QuestLog log;
    log.Accept(makeSimpleTemplate(1, 100, 1));
    log.Accept(makeSimpleTemplate(2, 200, 1));
    log.Accept(makeSimpleTemplate(3, 300, 5));

    // Complete and turn in quest 1.
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);

    // Fail quest 2.
    log.GetQuest(2)->state = QuestState::Failed;

    // Quest 3 is still active.
    log.CleanupFinished();

    EXPECT_EQ(log.activeQuests.size(), 1u);
    EXPECT_TRUE(log.HasQuest(3));
    EXPECT_FALSE(log.HasQuest(1));
    EXPECT_FALSE(log.HasQuest(2));
}

// =============================================================================
// Quest prerequisites and chains (SRS-GML-005.3)
// =============================================================================

TEST_F(QuestLogTest, CanAcceptChecksPrerequisites) {
    QuestLog log;

    QuestTemplate quest1 = makeSimpleTemplate(1, 100, 1);
    QuestTemplate quest2 = makeSimpleTemplate(2, 200, 1);
    quest2.prerequisites = {1};  // Requires quest 1 completed.

    // Cannot accept quest 2 without completing quest 1.
    EXPECT_FALSE(log.CanAccept(quest2));
    EXPECT_FALSE(log.Accept(quest2));

    // Complete quest 1.
    log.Accept(quest1);
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);

    // Now quest 2 is available.
    EXPECT_TRUE(log.CanAccept(quest2));
    EXPECT_TRUE(log.Accept(quest2));
}

TEST_F(QuestLogTest, CanAcceptBlocksAlreadyCompleted) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1, 100, 1);

    log.Accept(tmpl);
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);
    log.CleanupFinished();

    // Cannot accept again (non-repeatable).
    EXPECT_FALSE(log.CanAccept(tmpl));
    EXPECT_FALSE(log.Accept(tmpl));
}

TEST_F(QuestLogTest, RepeatableQuestCanBeAcceptedAgain) {
    QuestLog log;
    auto tmpl = makeSimpleTemplate(1, 100, 1);
    tmpl.flags = QuestFlags::Repeatable;

    log.Accept(tmpl);
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);
    log.CleanupFinished();

    // Can accept again.
    EXPECT_TRUE(log.CanAccept(tmpl));
    EXPECT_TRUE(log.Accept(tmpl));
}

TEST_F(QuestLogTest, QuestChainUnlocking) {
    QuestLog log;

    // Build a chain: quest 1 -> quest 2 -> quest 3.
    QuestTemplate q1 = makeSimpleTemplate(1, 100, 1);
    q1.chainNext = 2;

    QuestTemplate q2 = makeSimpleTemplate(2, 200, 1);
    q2.prerequisites = {1};
    q2.chainNext = 3;

    QuestTemplate q3 = makeSimpleTemplate(3, 300, 1);
    q3.prerequisites = {2};

    // Only quest 1 is available initially.
    EXPECT_TRUE(log.CanAccept(q1));
    EXPECT_FALSE(log.CanAccept(q2));
    EXPECT_FALSE(log.CanAccept(q3));

    // Complete quest 1 -> unlocks quest 2.
    log.Accept(q1);
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);
    log.CleanupFinished();

    EXPECT_TRUE(log.CanAccept(q2));
    EXPECT_FALSE(log.CanAccept(q3));

    // Complete quest 2 -> unlocks quest 3.
    log.Accept(q2);
    log.GetQuest(2)->UpdateObjective(ObjectiveType::Kill, 200);
    log.TurnIn(2);
    log.CleanupFinished();

    EXPECT_TRUE(log.CanAccept(q3));
}

TEST_F(QuestLogTest, MultiplePrerequisitesAllRequired) {
    QuestLog log;

    QuestTemplate q1 = makeSimpleTemplate(1, 100, 1);
    QuestTemplate q2 = makeSimpleTemplate(2, 200, 1);
    QuestTemplate q3 = makeSimpleTemplate(3, 300, 1);
    q3.prerequisites = {1, 2};  // Both quest 1 and 2 required.

    // Complete only quest 1.
    log.Accept(q1);
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    log.TurnIn(1);
    log.CleanupFinished();

    EXPECT_FALSE(log.CanAccept(q3));  // Still needs quest 2.

    // Complete quest 2.
    log.Accept(q2);
    log.GetQuest(2)->UpdateObjective(ObjectiveType::Kill, 200);
    log.TurnIn(2);
    log.CleanupFinished();

    EXPECT_TRUE(log.CanAccept(q3));
}

// =============================================================================
// QuestFlags tests
// =============================================================================

TEST(QuestFlagsTest, BitwiseOperations) {
    auto combined = QuestFlags::Repeatable | QuestFlags::Daily;
    EXPECT_TRUE(HasQuestFlag(combined, QuestFlags::Repeatable));
    EXPECT_TRUE(HasQuestFlag(combined, QuestFlags::Daily));
    EXPECT_FALSE(HasQuestFlag(combined, QuestFlags::Weekly));
    EXPECT_FALSE(HasQuestFlag(QuestFlags::None, QuestFlags::Timed));
}

// =============================================================================
// QuestSystem tests (SRS-GML-005.4)
// =============================================================================

class QuestSystemTest : public ::testing::Test {
protected:
    ComponentStorage<QuestLog> questLogs;
    ComponentStorage<QuestEvent> questEvents;

    Entity player{0, 0};

    QuestTemplate makeKillTemplate(uint32_t id, uint32_t creature,
                                   int32_t count) {
        QuestTemplate tmpl;
        tmpl.id = id;
        tmpl.name = "Kill Quest " + std::to_string(id);
        QuestObjective obj;
        obj.type = ObjectiveType::Kill;
        obj.targetId = creature;
        obj.required = count;
        tmpl.objectives = {obj};
        tmpl.rewards.experience = 100;
        return tmpl;
    }

    void SetUp() override {
        questLogs.Add(player);
    }
};

TEST_F(QuestSystemTest, ProcessKillEvent) {
    auto& log = questLogs.Get(player);
    auto tmpl = makeKillTemplate(1, 100, 3);
    log.Accept(tmpl);

    // Create a kill event.
    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = player;
    event.type = QuestEventType::Kill;
    event.targetId = 100;
    event.count = 1;
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    // Objective should have progressed.
    EXPECT_EQ(log.GetQuest(1)->objectives[0].current, 1);
    EXPECT_TRUE(questEvents.Get(eventEntity).processed);
}

TEST_F(QuestSystemTest, ProcessCollectEvent) {
    auto& log = questLogs.Get(player);

    QuestTemplate tmpl;
    tmpl.id = 2;
    tmpl.name = "Collect Quest";
    QuestObjective obj;
    obj.type = ObjectiveType::Collect;
    obj.targetId = 500;
    obj.required = 5;
    tmpl.objectives = {obj};
    log.Accept(tmpl);

    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = player;
    event.type = QuestEventType::Collect;
    event.targetId = 500;
    event.count = 3;
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    EXPECT_EQ(log.GetQuest(2)->objectives[0].current, 3);
}

TEST_F(QuestSystemTest, ProcessExploreEvent) {
    auto& log = questLogs.Get(player);

    QuestTemplate tmpl;
    tmpl.id = 3;
    tmpl.name = "Explore Quest";
    QuestObjective obj;
    obj.type = ObjectiveType::Explore;
    obj.targetId = 42;
    obj.required = 1;
    tmpl.objectives = {obj};
    log.Accept(tmpl);

    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = player;
    event.type = QuestEventType::Explore;
    event.targetId = 42;
    event.count = 1;
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    EXPECT_EQ(log.GetQuest(3)->state, QuestState::ObjectivesComplete);
}

TEST_F(QuestSystemTest, ProcessInteractEvent) {
    auto& log = questLogs.Get(player);

    QuestTemplate tmpl;
    tmpl.id = 4;
    tmpl.name = "Interact Quest";
    QuestObjective obj;
    obj.type = ObjectiveType::Interact;
    obj.targetId = 77;
    obj.required = 1;
    tmpl.objectives = {obj};
    log.Accept(tmpl);

    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = player;
    event.type = QuestEventType::Interact;
    event.targetId = 77;
    event.count = 1;
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    EXPECT_EQ(log.GetQuest(4)->state, QuestState::ObjectivesComplete);
}

TEST_F(QuestSystemTest, EventForUnknownPlayerIsIgnored) {
    auto& log = questLogs.Get(player);
    auto tmpl = makeKillTemplate(1, 100, 3);
    log.Accept(tmpl);

    // Event for a different player who has no QuestLog.
    Entity otherPlayer(99, 0);
    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = otherPlayer;
    event.type = QuestEventType::Kill;
    event.targetId = 100;
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    // Original player's quest should be unaffected.
    EXPECT_EQ(log.GetQuest(1)->objectives[0].current, 0);
    EXPECT_TRUE(questEvents.Get(eventEntity).processed);
}

TEST_F(QuestSystemTest, AlreadyProcessedEventIsSkipped) {
    auto& log = questLogs.Get(player);
    auto tmpl = makeKillTemplate(1, 100, 5);
    log.Accept(tmpl);

    Entity eventEntity(10, 0);
    QuestEvent event;
    event.player = player;
    event.type = QuestEventType::Kill;
    event.targetId = 100;
    event.processed = true;  // Already processed.
    questEvents.Add(eventEntity, std::move(event));

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    EXPECT_EQ(log.GetQuest(1)->objectives[0].current, 0);
}

TEST_F(QuestSystemTest, MultipleEventsInSingleTick) {
    auto& log = questLogs.Get(player);
    auto tmpl = makeKillTemplate(1, 100, 3);
    log.Accept(tmpl);

    // Create three kill events.
    for (uint32_t i = 0; i < 3; ++i) {
        Entity eventEntity(10 + i, 0);
        QuestEvent event;
        event.player = player;
        event.type = QuestEventType::Kill;
        event.targetId = 100;
        questEvents.Add(eventEntity, std::move(event));
    }

    QuestSystem system(questLogs, questEvents);
    system.Execute(0.016f);

    EXPECT_EQ(log.GetQuest(1)->objectives[0].current, 3);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::ObjectivesComplete);
}

// =============================================================================
// Timed quest tests
// =============================================================================

TEST_F(QuestSystemTest, TimedQuestExpires) {
    auto& log = questLogs.Get(player);

    QuestTemplate tmpl = makeKillTemplate(1, 100, 5);
    tmpl.timeLimitSeconds = 10.0f;
    tmpl.flags = QuestFlags::Timed;
    log.Accept(tmpl);

    QuestSystem system(questLogs, questEvents);

    // Tick 5 seconds — still active.
    system.Execute(5.0f);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::Accepted);
    EXPECT_NEAR(log.GetQuest(1)->elapsedTime, 5.0f, 1e-5f);

    // Tick 6 more seconds — should expire.
    system.Execute(6.0f);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::Failed);
}

TEST_F(QuestSystemTest, NonTimedQuestDoesNotExpire) {
    auto& log = questLogs.Get(player);
    auto tmpl = makeKillTemplate(1, 100, 5);
    // timeLimit defaults to 0 (no limit).
    log.Accept(tmpl);

    QuestSystem system(questLogs, questEvents);
    system.Execute(1000.0f);

    EXPECT_EQ(log.GetQuest(1)->state, QuestState::Accepted);
    EXPECT_FLOAT_EQ(log.GetQuest(1)->elapsedTime, 0.0f);
}

TEST_F(QuestSystemTest, CompletedQuestTimerDoesNotTick) {
    auto& log = questLogs.Get(player);

    QuestTemplate tmpl = makeKillTemplate(1, 100, 1);
    tmpl.timeLimitSeconds = 5.0f;
    log.Accept(tmpl);

    // Complete the quest objective.
    log.GetQuest(1)->UpdateObjective(ObjectiveType::Kill, 100);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::ObjectivesComplete);

    QuestSystem system(questLogs, questEvents);

    // Tick past the time limit — should NOT fail since objectives are complete.
    system.Execute(10.0f);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::ObjectivesComplete);
}

// =============================================================================
// QuestSystem template registry
// =============================================================================

TEST(QuestSystemTemplateTest, RegisterAndGetTemplate) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);

    QuestTemplate tmpl;
    tmpl.id = 42;
    tmpl.name = "Dragon Slayer";
    tmpl.rewards.experience = 5000;
    system.RegisterTemplate(std::move(tmpl));

    const auto* found = system.GetTemplate(42);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Dragon Slayer");
    EXPECT_EQ(found->rewards.experience, 5000);
}

TEST(QuestSystemTemplateTest, GetNonexistentTemplate) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);
    EXPECT_EQ(system.GetTemplate(999), nullptr);
}

TEST(QuestSystemTemplateTest, RegisterTemplateOverwritesExisting) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);

    QuestTemplate tmpl1;
    tmpl1.id = 1;
    tmpl1.name = "Version 1";
    system.RegisterTemplate(std::move(tmpl1));

    QuestTemplate tmpl2;
    tmpl2.id = 1;
    tmpl2.name = "Version 2";
    system.RegisterTemplate(std::move(tmpl2));

    EXPECT_EQ(system.GetTemplate(1)->name, "Version 2");
}

// =============================================================================
// QuestSystem metadata
// =============================================================================

TEST(QuestSystemMetaTest, SystemStageIsPostUpdate) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);
    EXPECT_EQ(system.GetStage(), SystemStage::PostUpdate);
}

TEST(QuestSystemMetaTest, SystemName) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);
    EXPECT_EQ(system.GetName(), "QuestSystem");
}

TEST(QuestSystemMetaTest, AccessInfoDeclaresWriteAccess) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    QuestSystem system(logs, events);
    auto info = system.GetAccessInfo();

    EXPECT_TRUE(info.writes.count(ComponentType<QuestLog>::Id()));
    EXPECT_TRUE(info.writes.count(ComponentType<QuestEvent>::Id()));
}

// =============================================================================
// Integration: full quest scenario
// =============================================================================

TEST(QuestIntegration, FullQuestLifecycle) {
    ComponentStorage<QuestLog> questLogs;
    ComponentStorage<QuestEvent> questEvents;

    EntityManager entityManager;
    entityManager.RegisterStorage(&questLogs);
    entityManager.RegisterStorage(&questEvents);

    // Create a player.
    Entity player = entityManager.Create();
    questLogs.Add(player);
    auto& log = questLogs.Get(player);

    // Define a quest: kill 3 wolves and collect 2 pelts.
    QuestTemplate wolfHunt;
    wolfHunt.id = 10;
    wolfHunt.name = "Wolf Hunt";
    wolfHunt.level = 5;

    QuestObjective killWolves;
    killWolves.type = ObjectiveType::Kill;
    killWolves.targetId = 1001;  // Wolf creature entry.
    killWolves.required = 3;

    QuestObjective collectPelts;
    collectPelts.type = ObjectiveType::Collect;
    collectPelts.targetId = 2001;  // Wolf pelt item ID.
    collectPelts.required = 2;

    wolfHunt.objectives = {killWolves, collectPelts};
    wolfHunt.rewards.experience = 250;
    wolfHunt.rewards.currency = 100;
    wolfHunt.rewards.items = {{2001, 1}};  // Bonus pelt.

    QuestSystem system(questLogs, questEvents);
    system.RegisterTemplate(wolfHunt);

    // Phase 1: Accept the quest.
    EXPECT_TRUE(log.Accept(wolfHunt));
    EXPECT_EQ(log.activeQuests.size(), 1u);
    EXPECT_EQ(log.GetQuest(10)->state, QuestState::Accepted);

    // Phase 2: Kill 2 wolves via events.
    for (int i = 0; i < 2; ++i) {
        Entity evt = entityManager.Create();
        QuestEvent killEvent;
        killEvent.player = player;
        killEvent.type = QuestEventType::Kill;
        killEvent.targetId = 1001;
        questEvents.Add(evt, std::move(killEvent));
    }

    system.Execute(0.016f);

    auto* quest = log.GetQuest(10);
    ASSERT_NE(quest, nullptr);
    EXPECT_EQ(quest->objectives[0].current, 2);  // 2/3 wolves.
    EXPECT_EQ(quest->objectives[1].current, 0);  // 0/2 pelts.
    EXPECT_EQ(quest->state, QuestState::Accepted);

    // Phase 3: Kill the last wolf + collect 2 pelts.
    Entity killEvt = entityManager.Create();
    QuestEvent lastKill;
    lastKill.player = player;
    lastKill.type = QuestEventType::Kill;
    lastKill.targetId = 1001;
    questEvents.Add(killEvt, std::move(lastKill));

    for (int i = 0; i < 2; ++i) {
        Entity collectEvt = entityManager.Create();
        QuestEvent collectEvent;
        collectEvent.player = player;
        collectEvent.type = QuestEventType::Collect;
        collectEvent.targetId = 2001;
        questEvents.Add(collectEvt, std::move(collectEvent));
    }

    system.Execute(0.016f);

    EXPECT_EQ(quest->objectives[0].current, 3);  // 3/3 wolves.
    EXPECT_EQ(quest->objectives[1].current, 2);  // 2/2 pelts.
    EXPECT_EQ(quest->state, QuestState::ObjectivesComplete);

    // Phase 4: Turn in the quest.
    EXPECT_TRUE(log.TurnIn(10));
    EXPECT_EQ(quest->state, QuestState::TurnedIn);
    EXPECT_TRUE(log.IsCompleted(10));

    // Phase 5: Cleanup.
    log.CleanupFinished();
    EXPECT_TRUE(log.activeQuests.empty());
    EXPECT_TRUE(log.IsCompleted(10));

    // Clean up entities.
    entityManager.Destroy(player);
}

TEST(QuestIntegration, QuestChainWithEvents) {
    ComponentStorage<QuestLog> questLogs;
    ComponentStorage<QuestEvent> questEvents;

    EntityManager entityManager;
    entityManager.RegisterStorage(&questLogs);
    entityManager.RegisterStorage(&questEvents);

    Entity player = entityManager.Create();
    questLogs.Add(player);
    auto& log = questLogs.Get(player);

    // Chain: quest 1 (kill 1) -> quest 2 (explore 1).
    QuestTemplate q1;
    q1.id = 1;
    q1.name = "Slay the Beast";
    q1.chainNext = 2;
    QuestObjective killObj;
    killObj.type = ObjectiveType::Kill;
    killObj.targetId = 500;
    killObj.required = 1;
    q1.objectives = {killObj};
    q1.rewards.experience = 100;

    QuestTemplate q2;
    q2.id = 2;
    q2.name = "Explore the Lair";
    q2.prerequisites = {1};
    QuestObjective exploreObj;
    exploreObj.type = ObjectiveType::Explore;
    exploreObj.targetId = 10;
    exploreObj.required = 1;
    q2.objectives = {exploreObj};
    q2.rewards.experience = 200;

    QuestSystem system(questLogs, questEvents);
    system.RegisterTemplate(q1);
    system.RegisterTemplate(q2);

    // Quest 2 not yet available.
    EXPECT_FALSE(log.CanAccept(q2));

    // Accept and complete quest 1.
    log.Accept(q1);

    Entity killEvt = entityManager.Create();
    QuestEvent ke;
    ke.player = player;
    ke.type = QuestEventType::Kill;
    ke.targetId = 500;
    questEvents.Add(killEvt, std::move(ke));

    system.Execute(0.016f);
    EXPECT_EQ(log.GetQuest(1)->state, QuestState::ObjectivesComplete);

    log.TurnIn(1);
    log.CleanupFinished();

    // Quest 2 now available.
    EXPECT_TRUE(log.CanAccept(q2));
    log.Accept(q2);

    Entity exploreEvt = entityManager.Create();
    QuestEvent ee;
    ee.player = player;
    ee.type = QuestEventType::Explore;
    ee.targetId = 10;
    questEvents.Add(exploreEvt, std::move(ee));

    system.Execute(0.016f);
    EXPECT_EQ(log.GetQuest(2)->state, QuestState::ObjectivesComplete);

    log.TurnIn(2);
    EXPECT_TRUE(log.IsCompleted(1));
    EXPECT_TRUE(log.IsCompleted(2));

    entityManager.Destroy(player);
}

TEST(QuestIntegration, SchedulerRegistration) {
    ComponentStorage<QuestLog> logs;
    ComponentStorage<QuestEvent> events;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<QuestSystem>(logs, events);

    EXPECT_EQ(system.GetName(), "QuestSystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}
