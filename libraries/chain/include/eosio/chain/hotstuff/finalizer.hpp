#pragma once
#include <eosio/chain/hotstuff/hotstuff.hpp>
#include <eosio/chain/fork_database.hpp>

namespace eosio::chain {
   using fork_db_t = fork_database<block_state_ptr>;

   struct qc_chain {
      block_state_ptr b2; // first phase,  prepare
      block_state_ptr b1; // second phase, precommit
      block_state_ptr b;  // third phase,  commit
   };

   struct finalizer {
      enum class VoteDecision { StrongVote, WeakVote, NoVote };

      struct time_range_t {
         block_timestamp_type start;
         block_timestamp_type end;
      };

      struct safety_information {
         time_range_t         last_vote_range;
         block_id_type        last_vote;          // v_height under hotstuff
         block_id_type        lock_id;            // b_lock under hotstuff
         bool                 is_last_vote_strong;
         bool                 recovery_mode;
      };

      bls_public_key      pub_key;
      bls_private_key     priv_key;
      safety_information  fsi;

   private:
      qc_chain     get_qc_chain(const block_state_ptr&  proposal, const fork_db_t& fork_db) const;
      VoteDecision decide_vote(const block_state_ptr& proposal, const fork_db_t& fork_db) const;
   };

}