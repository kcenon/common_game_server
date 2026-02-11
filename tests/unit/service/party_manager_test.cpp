/// @file party_manager_test.cpp
/// @brief Unit tests for PartyManager (party lifecycle, member management).

#include <gtest/gtest.h>

#include "cgs/foundation/error_code.hpp"
#include "cgs/service/lobby_types.hpp"
#include "cgs/service/party_manager.hpp"

using namespace cgs::service;
using cgs::foundation::ErrorCode;

// ============================================================================
// PartyManager Tests
// ============================================================================

class PartyManagerTest : public ::testing::Test {
protected:
    PartyManager pm_{5};

    PlayerRating makeRating(int32_t mmr) {
        PlayerRating r;
        r.mmr = mmr;
        return r;
    }
};

// -- Party lifecycle ----------------------------------------------------------

TEST_F(PartyManagerTest, CreatePartySucceeds) {
    auto result = pm_.createParty(1, "Alice", makeRating(1500));
    ASSERT_TRUE(result.hasValue());
    EXPECT_GT(result.value(), 0u);
    EXPECT_EQ(pm_.partyCount(), 1u);
}

TEST_F(PartyManagerTest, CreatePartyPlayerAlreadyInParty) {
    (void)pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.createParty(1, "Alice", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerAlreadyInParty);
}

TEST_F(PartyManagerTest, CreatePartyLeaderIsMember) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    ASSERT_TRUE(partyId.hasValue());

    auto party = pm_.getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_EQ(party->leaderId, 1u);
    EXPECT_EQ(party->members.size(), 1u);
    EXPECT_EQ(party->members[0].playerId, 1u);
    EXPECT_EQ(party->members[0].name, "Alice");
}

TEST_F(PartyManagerTest, DisbandPartySucceeds) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.disbandParty(partyId.value(), 1);
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(pm_.partyCount(), 0u);
    EXPECT_FALSE(pm_.isInParty(1));
}

TEST_F(PartyManagerTest, DisbandPartyNotLeaderFails) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm_.disbandParty(partyId.value(), 2);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotPartyLeader);
}

TEST_F(PartyManagerTest, DisbandPartyNotFoundFails) {
    auto result = pm_.disbandParty(999, 1);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

TEST_F(PartyManagerTest, DisbandClearsAllMemberMappings) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 3, "Charlie", makeRating(1500));

    (void)pm_.disbandParty(partyId.value(), 1);

    EXPECT_FALSE(pm_.isInParty(1));
    EXPECT_FALSE(pm_.isInParty(2));
    EXPECT_FALSE(pm_.isInParty(3));
}

// -- Member management --------------------------------------------------------

TEST_F(PartyManagerTest, AddMemberSucceeds) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.addMember(partyId.value(), 2, "Bob", makeRating(1600));

    EXPECT_TRUE(result.hasValue());

    auto party = pm_.getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_EQ(party->members.size(), 2u);
}

TEST_F(PartyManagerTest, AddMemberPartyNotFound) {
    auto result = pm_.addMember(999, 2, "Bob", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

TEST_F(PartyManagerTest, AddMemberAlreadyInParty) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.addMember(partyId.value(), 1, "Alice", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerAlreadyInParty);
}

TEST_F(PartyManagerTest, AddMemberAlreadyInDifferentParty) {
    auto p1 = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.createParty(2, "Bob", makeRating(1500));

    auto result = pm_.addMember(p1.value(), 2, "Bob", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerAlreadyInParty);
}

TEST_F(PartyManagerTest, AddMemberPartyFull) {
    PartyManager pm(2);
    auto partyId = pm.createParty(1, "Alice", makeRating(1500));
    (void)pm.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm.addMember(partyId.value(), 3, "Charlie", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyFull);
}

TEST_F(PartyManagerTest, RemoveMemberNonLeader) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm_.removeMember(partyId.value(), 2);
    EXPECT_TRUE(result.hasValue());

    auto party = pm_.getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_EQ(party->members.size(), 1u);
    EXPECT_FALSE(pm_.isInParty(2));
}

TEST_F(PartyManagerTest, RemoveLeaderDisbandsParty) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm_.removeMember(partyId.value(), 1);
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(pm_.partyCount(), 0u);
    EXPECT_FALSE(pm_.isInParty(1));
    EXPECT_FALSE(pm_.isInParty(2));
}

TEST_F(PartyManagerTest, RemoveMemberNotInParty) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.removeMember(partyId.value(), 99);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerNotInParty);
}

TEST_F(PartyManagerTest, RemoveMemberPartyNotFound) {
    auto result = pm_.removeMember(999, 1);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

// -- Promote leader -----------------------------------------------------------

TEST_F(PartyManagerTest, PromoteLeaderSucceeds) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm_.promoteLeader(partyId.value(), 1, 2);
    EXPECT_TRUE(result.hasValue());

    auto party = pm_.getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_EQ(party->leaderId, 2u);
}

TEST_F(PartyManagerTest, PromoteLeaderNotCurrentLeader) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = pm_.promoteLeader(partyId.value(), 2, 1);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::NotPartyLeader);
}

TEST_F(PartyManagerTest, PromoteLeaderTargetNotInParty) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.promoteLeader(partyId.value(), 1, 99);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerNotInParty);
}

TEST_F(PartyManagerTest, PromoteLeaderPartyNotFound) {
    auto result = pm_.promoteLeader(999, 1, 2);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

// -- Queue integration --------------------------------------------------------

TEST_F(PartyManagerTest, SetInQueueSucceeds) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto result = pm_.setInQueue(partyId.value(), true);
    EXPECT_TRUE(result.hasValue());

    auto party = pm_.getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_TRUE(party->inQueue);
}

TEST_F(PartyManagerTest, SetInQueuePartyNotFound) {
    auto result = pm_.setInQueue(999, true);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

// -- Queries ------------------------------------------------------------------

TEST_F(PartyManagerTest, GetPlayerParty) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto p1 = pm_.getPlayerParty(1);
    auto p2 = pm_.getPlayerParty(2);
    ASSERT_TRUE(p1.has_value());
    ASSERT_TRUE(p2.has_value());
    EXPECT_EQ(*p1, partyId.value());
    EXPECT_EQ(*p2, partyId.value());
}

TEST_F(PartyManagerTest, GetPlayerPartyNotInAny) {
    auto result = pm_.getPlayerParty(999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PartyManagerTest, IsInPartyCheck) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    EXPECT_TRUE(pm_.isInParty(1));
    EXPECT_FALSE(pm_.isInParty(2));

    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1500));
    EXPECT_TRUE(pm_.isInParty(2));
}

TEST_F(PartyManagerTest, AveragePartyRating) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.addMember(partyId.value(), 2, "Bob", makeRating(1600));
    (void)pm_.addMember(partyId.value(), 3, "Charlie", makeRating(1700));

    auto avg = pm_.averagePartyRating(partyId.value());
    ASSERT_TRUE(avg.has_value());
    EXPECT_EQ(*avg, 1600); // (1500 + 1600 + 1700) / 3
}

TEST_F(PartyManagerTest, AveragePartyRatingSingleMember) {
    auto partyId = pm_.createParty(1, "Alice", makeRating(1500));
    auto avg = pm_.averagePartyRating(partyId.value());
    ASSERT_TRUE(avg.has_value());
    EXPECT_EQ(*avg, 1500);
}

TEST_F(PartyManagerTest, AveragePartyRatingNotFound) {
    auto avg = pm_.averagePartyRating(999);
    EXPECT_FALSE(avg.has_value());
}

TEST_F(PartyManagerTest, UniquePartyIds) {
    auto p1 = pm_.createParty(1, "Alice", makeRating(1500));
    (void)pm_.disbandParty(p1.value(), 1);
    auto p2 = pm_.createParty(1, "Alice", makeRating(1500));

    ASSERT_TRUE(p1.hasValue());
    ASSERT_TRUE(p2.hasValue());
    EXPECT_NE(p1.value(), p2.value());
}

TEST_F(PartyManagerTest, MultiplePartiesCoexist) {
    auto p1 = pm_.createParty(1, "Alice", makeRating(1500));
    auto p2 = pm_.createParty(2, "Bob", makeRating(1600));

    EXPECT_EQ(pm_.partyCount(), 2u);

    auto party1 = pm_.getParty(p1.value());
    auto party2 = pm_.getParty(p2.value());
    ASSERT_TRUE(party1.has_value());
    ASSERT_TRUE(party2.has_value());
    EXPECT_EQ(party1->leaderId, 1u);
    EXPECT_EQ(party2->leaderId, 2u);
}

TEST_F(PartyManagerTest, FillPartyToMaxSize) {
    PartyManager pm(3);
    auto partyId = pm.createParty(1, "P1", makeRating(1500));

    auto r2 = pm.addMember(partyId.value(), 2, "P2", makeRating(1500));
    EXPECT_TRUE(r2.hasValue());

    auto r3 = pm.addMember(partyId.value(), 3, "P3", makeRating(1500));
    EXPECT_TRUE(r3.hasValue());

    auto r4 = pm.addMember(partyId.value(), 4, "P4", makeRating(1500));
    EXPECT_TRUE(r4.hasError());
    EXPECT_EQ(r4.error().code(), ErrorCode::PartyFull);
}
