#include "finality_test_cluster.hpp"

/*
 * register test suite `finality_tests`
 */
BOOST_AUTO_TEST_SUITE(finality_tests)

// verify LIB advances with 2 finalizers voting.
BOOST_FIXTURE_TEST_CASE(two_votes, finality_test_cluster) { try {
   produce_and_push_block();
   for (auto i = 0; i < 3; ++i) {
      process_votes();
      produce_and_push_block(); // so all nodes receive the new QC with votes
      BOOST_REQUIRE_EQUAL(lib_advancing(), num_nodes);
   }
} FC_LOG_AND_RETHROW() }

// verify LIB does not advances with finalizers not voting.
BOOST_FIXTURE_TEST_CASE(no_votes, finality_test_cluster) { try {
   BOOST_REQUIRE_EQUAL(lib_advancing(), 0);
   produce_and_push_block();
   for (auto i = 0; i < 3; ++i) {
      // don't process votes
      produce_and_push_block();

      // LIB shouldn't advance on any node
      BOOST_REQUIRE_EQUAL(lib_advancing(), 0);
   }
} FC_LOG_AND_RETHROW() }

#if 0
// verify LIB advances with all of the three finalizers voting
BOOST_FIXTURE_TEST_CASE(all_votes, finality_test_cluster) { try {

   produce_and_push_block();
   for (auto i = 0; i < 3; ++i) {
      // process node1 and node2's votes
      process_votes(num_nodes - 1);
      node1.process_vote(*this);
      node2.process_vote(*this);

      // node0 produces a block and pushes to node1 and node2
      produce_and_push_block();

      // all nodes advance LIB
      BOOST_REQUIRE(lib_advancing());
   }
} FC_LOG_AND_RETHROW() }

// verify LIB advances when votes conflict (strong first and followed by weak)
BOOST_FIXTURE_TEST_CASE(conflicting_votes_strong_first, finality_test_cluster) { try {
   produce_and_push_block();
   for (auto i = 0; i < 3; ++i) {
      node1.process_vote(*this);  // strong
      node2.process_vote(*this, -1, vote_mode::weak); // weak
      produce_and_push_block();

      BOOST_REQUIRE(lib_advancing());
   }
} FC_LOG_AND_RETHROW() }

// verify LIB advances when votes conflict (weak first and followed by strong)
BOOST_FIXTURE_TEST_CASE(conflicting_votes_weak_first, finality_test_cluster) { try {
   produce_and_push_block();
   for (auto i = 0; i < 3; ++i) {
      node1.process_vote(*this, -1, vote_mode::weak);  // weak
      node2.process_vote(*this);  // strong
      produce_and_push_block();

      BOOST_REQUIRE(lib_advancing());
   }
} FC_LOG_AND_RETHROW() }

// Verify a delayed vote works
BOOST_FIXTURE_TEST_CASE(one_delayed_votes, finality_test_cluster) { try {
   // hold the vote for the first block to simulate delay
   produce_and_push_block();
   // LIB advanced on nodes because a new block was received
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   produce_and_push_block();
   // vote block 0 (index 0) to make it have a strong QC,
   // prompting LIB advacing on node2
   node1.process_vote(*this, 0);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // block 1 (index 1) has the same QC claim as block 0. It cannot move LIB
   node1.process_vote(*this, 1);
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // producing, pushing, and voting a new block makes LIB moving
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// Verify 3 consecutive delayed votes work
BOOST_FIXTURE_TEST_CASE(three_delayed_votes, finality_test_cluster) { try {
   // produce 4 blocks and hold the votes for the first 3 to simulate delayed votes
   // The 4 blocks have the same QC claim as no QCs are created because missing one vote
   for (auto i = 0; i < 4; ++i) {
      produce_and_push_block();
   }
   // LIB advanced on nodes because a new block was received
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // vote block 0 (index 0) to make it have a strong QC,
   // prompting LIB advacing on nodes
   node1.process_vote(*this, 0);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // blocks 1 to 3 have the same QC claim as block 0. It cannot move LIB
   for (auto i=1; i < 4; ++i) {
      node1.process_vote(*this, i);
      produce_and_push_block();
      BOOST_REQUIRE(!node2.lib_advancing());
      BOOST_REQUIRE(!node1.lib_advancing());
   }

   // producing, pushing, and voting a new block makes LIB moving
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(out_of_order_votes, finality_test_cluster) { try {
   // produce 3 blocks and hold the votes to simulate delayed votes
   // The 3 blocks have the same QC claim as no QCs are created because missing votes
   for (auto i = 0; i < 3; ++i) {
      produce_and_push_block();
   }

   // vote out of the order: the newest to oldest

   // vote block 2 (index 2) to make it have a strong QC,
   // prompting LIB advacing
   node1.process_vote(*this, 2);
   produce_and_push_block();
   BOOST_REQUIRE(node0.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // block 1 (index 1) has the same QC claim as block 2. It will not move LIB
   node1.process_vote(*this, 1);
   produce_and_push_block();
   BOOST_REQUIRE(!node0.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // block 0 (index 0) has the same QC claim as block 2. It will not move LIB
   node1.process_vote(*this, 0);
   produce_and_push_block();
   BOOST_REQUIRE(!node0.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // producing, pushing, and voting a new block makes LIB moving
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node0.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// Verify a vote which was delayed by a large number of blocks does not cause any issues
BOOST_FIXTURE_TEST_CASE(long_delayed_votes, finality_test_cluster) { try {
   // Produce and push a block, vote on it after a long delay.
   constexpr uint32_t delayed_vote_index = 0;
   produce_and_push_block();
   // The strong QC extension for prior block makes LIB advance on nodes
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   // the vote makes a strong QC for the current block, prompting LIB advance on nodes
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   for (auto i = 2; i < 100; ++i) {
      node1.process_vote(*this);
      produce_and_push_block();
      BOOST_REQUIRE(node0.lib_advancing());
      BOOST_REQUIRE(node1.lib_advancing());
   }

   // Late vote does not cause any issues
   BOOST_REQUIRE_NO_THROW(node1.process_vote(*this, delayed_vote_index));

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(lost_votes, finality_test_cluster) { try {
   // Produce and push a block, never vote on it to simulate lost.
   // The block contains a strong QC extension for prior block
   produce_and_push_block();

   // The strong QC extension for prior block makes LIB advance on nodes
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   produce_and_push_block();
   // The block is not voted, so no strong QC is created and LIB does not advance on nodes
   BOOST_REQUIRE(!node1.lib_advancing());
   BOOST_REQUIRE(!node2.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();

   // vote causes lib to advance
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(one_weak_vote, finality_test_cluster) { try {
   // Produce and push a block
   produce_and_push_block();
   // Change the vote to a weak vote and process it
   node1.process_vote(*this, 0, vote_mode::weak);
   // The strong QC extension for prior block makes LIB advance on node1
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   produce_and_push_block();
   // A weak QC is created and LIB does not advance on node2
   BOOST_REQUIRE(!node2.lib_advancing());
   // no 2-chain was formed as prior block was not a strong block
   BOOST_REQUIRE(!node1.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   // the vote makes a strong QC and a higher final_on_strong_qc,
   // prompting LIB advance on nodes
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   // now a 3 chain has formed.
   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(two_weak_votes, finality_test_cluster) { try {
   // Produce and push a block
   produce_and_push_block();
   // The strong QC extension for prior block makes LIB advance on nodes
   BOOST_REQUIRE(node1.lib_advancing());
   BOOST_REQUIRE(node2.lib_advancing());

   // Change the vote to a weak vote and process it
   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();
   // A weak QC cannot advance LIB on nodes
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();
   // A weak QC cannot advance LIB on node2
   BOOST_REQUIRE(!node2.lib_advancing());
   // no 2-chain was formed as prior block was not a strong block
   BOOST_REQUIRE(!node1.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // now a 3 chain has formed.
   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(intertwined_weak_votes, finality_test_cluster) { try {
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // Weak vote
   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();

   // The strong QC extension for prior block makes LIB advance on nodes
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // Strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // Weak vote
   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();
   // A weak QC cannot advance LIB on nodes
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // Strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   // the vote makes a strong QC for the current block, prompting LIB advance on node0
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // Strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// Verify a combination of weak, delayed, lost votes still work
BOOST_FIXTURE_TEST_CASE(weak_delayed_lost_vote, finality_test_cluster) { try {
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // A weak vote
   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // A delayed vote (index 1)
   constexpr uint32_t delayed_index = 1; 
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // A strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // A lost vote
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // The delayed vote arrives, does not advance lib because it is weak
   node1.process_vote(*this, delayed_index);
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // strong vote advances lib
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// Verify a combination of delayed, weak, lost votes still work
BOOST_FIXTURE_TEST_CASE(delayed_strong_weak_lost_vote, finality_test_cluster) { try {
   // A delayed vote (index 0)
   constexpr uint32_t delayed_index = 0; 
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // A strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // A weak vote
   node1.process_vote(*this, -1, vote_mode::weak);
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // A strong vote
   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   // A lost vote
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   // The delayed vote arrives
   node1.process_vote(*this, delayed_index, vote_mode::strong, true);
   produce_and_push_block();
   BOOST_REQUIRE(!node2.lib_advancing());
   BOOST_REQUIRE(!node1.lib_advancing());

   node1.process_vote(*this);
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());
   BOOST_REQUIRE(node1.lib_advancing());

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// verify duplicate votes do not affect LIB advancing
BOOST_FIXTURE_TEST_CASE(duplicate_votes, finality_test_cluster) { try {
   produce_and_push_block();
   for (auto i = 0; i < 5; ++i) {
      node1.process_vote(*this, i, vote_mode::strong);
      // vote again to make it duplicate
      BOOST_REQUIRE(node1.process_vote(*this, i, vote_mode::strong, true) == eosio::chain::vote_status::duplicate);
      produce_and_push_block();

      // verify duplicate votes do not affect LIB advancing
      BOOST_REQUIRE(node2.lib_advancing());
      BOOST_REQUIRE(node1.lib_advancing());
   }
} FC_LOG_AND_RETHROW() }

// verify unknown_proposal votes are handled properly
BOOST_FIXTURE_TEST_CASE(unknown_proposal_votes, finality_test_cluster) { try {
   // node0 produces a block and pushes to node1
   produce_and_push_block();
   // intentionally corrupt block_id in node1's vote
   node1.corrupt_vote_block_id();

   // process the corrupted vote
   BOOST_REQUIRE_THROW(node1.process_vote(*this, 0), fc::exception); // throws because it times out waiting on vote
   produce_and_push_block();
   BOOST_REQUIRE(node2.lib_advancing());

   // restore to original vote
   node1.restore_to_original_vote();

   // process the original vote. LIB should advance
   produce_and_push_block();
   node1.process_vote(*this, 0, vote_mode::strong, true);

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// verify unknown finalizer_key votes are handled properly
BOOST_FIXTURE_TEST_CASE(unknown_finalizer_key_votes, finality_test_cluster) { try {
   // node0 produces a block and pushes to node1
   produce_and_push_block();

   // intentionally corrupt finalizer_key in node1's vote
   node1.corrupt_vote_finalizer_key();

   // process the corrupted vote. LIB should not advance
   node1.process_vote(*this, 0);
   BOOST_REQUIRE(node1.process_vote(*this, 0) == eosio::chain::vote_status::unknown_public_key);

   // restore to original vote
   node1.restore_to_original_vote();

   // process the original vote. LIB should advance
   node1.process_vote(*this, 0);

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

// verify corrupted signature votes are handled properly
BOOST_FIXTURE_TEST_CASE(corrupted_signature_votes, finality_test_cluster) { try {
   // node0 produces a block and pushes to node1
   produce_and_push_block();

   // intentionally corrupt signature in node1's vote
   node1.corrupt_vote_signature();

   // process the corrupted vote. LIB should not advance
   BOOST_REQUIRE(node1.process_vote(*this, 0) == eosio::chain::vote_status::invalid_signature);

   // restore to original vote
   node1.restore_to_original_vote();

   // process the original vote. LIB should advance
   node1.process_vote(*this);

   BOOST_REQUIRE(produce_blocks_and_verify_lib_advancing());
} FC_LOG_AND_RETHROW() }

#endif

BOOST_AUTO_TEST_SUITE_END()
