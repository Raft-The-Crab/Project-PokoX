# 08 — Creator Economy and Pokoins

## Current proposed rules
- Pokoins are platform virtual currency.
- Task earnings are capped at 1,500 Pokoins per week, resetting weekly.
- Proposed game engagement reward: 5 Pokoins per 3 minutes of eligible play for the developer.
- Proposed platform fee: 15% of eligible developer revenue; developer share 85%.

These are product rules to validate, not an implemented financial system.

## Separate virtual currency from real money
Pokoins should initially be non-transferable, non-cash-equivalent platform points unless a legally reviewed programme explicitly changes this. Clearly disclose that Pokoins have no guaranteed cash value. Do not imply that developer Pokoins are withdrawable income.

Real-money purchases, refunds, taxes, payout identity checks and chargebacks require a separate financial architecture and legal review.

## Earning controls
For task rewards:
- Server-side eligibility checks.
- Weekly cap enforcement in a transaction.
- Idempotency keys to prevent duplicate grants.
- Anti-bot and multi-account signals.
- Clear task completion rules.
- Ledger entry for every grant/reversal.

For playtime-based developer rewards:
- Count only eligible, active sessions.
- Exclude AFK and repeated idle farming.
- Require meaningful game activity signals.
- Apply per-user, per-game and per-developer caps.
- Detect scripted clients, collusion and multi-account abuse.
- Maintain a platform-wide reward budget.
- Provide an appeal path for reward fraud flags.

The 5 Pokoins / 3 minutes rule could be expensive or exploitable. Model the maximum liability before launch and consider a daily cap or a limited promotional programme.

## Wallet
Use an append-only ledger:
- Transaction ID.
- User/developer ID.
- Amount and currency type.
- Reason/source.
- Timestamp.
- Related task/session/order.
- Reversal reference if applicable.

Do not directly overwrite a balance without a ledger entry. Derive or reconcile balances from ledger transactions.

## Spending sinks
Potential uses:
- Approved avatar accessories.
- Profile customization.
- Creator asset purchases if supported.
- Game-specific cosmetic purchases.
- Optional group customization.

Avoid pay-to-win mechanics in official competitive experiences.

## Platform fee
For eligible real-money transactions, the proposed split is 15% platform / 85% creator. Define whether the split is calculated before or after taxes, payment processor fees, refunds and chargebacks. The final contract and accounting rules need legal/accounting review.

## Economy integrity
- No user-to-user Pokoins transfers initially.
- No gambling, loot-box cash equivalents or misleading prize mechanics.
- Clear transaction history.
- Refund and reversal policy.
- Fraud detection and manual investigation.
