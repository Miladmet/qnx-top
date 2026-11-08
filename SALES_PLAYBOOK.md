# Solo Developer Sales & Inbound Consulting Playbook for `qnx-top`

This playbook provides exact email templates, qualification steps, and billing procedures to close your first **$2,950 – $5,800** engineering engagement without a sales team.

---

## 1. The Inbound Lead Qualification Flow (15 Minutes)

When an engineering manager or tech lead contacts you via email or your booking link, qualify them with 4 quick questions:

```
[Inbound Email Received]
         │
         ▼
[Reply within 4 hours using Template A]
         │
         ▼
[They provide QNX version & architecture details]
         │
         ▼
[Send Mutual NDA + Fixed-Scope Scope of Work (SOW)]
         │
         ▼
[Collect 50% upfront via Stripe Invoice or Wire] ──► Work Begins
```

---

## 2. Copy-and-Paste Email Templates

### Template A: Rapid Reply to Inbound Inquiry
**Subject:** `Re: [Project Name] - QNX 8.0 Systems Advisory & Audit`

> Hi [Name],
>
> Thanks for reaching out. 
> 
> We specialize in hard real-time latency optimization, microkernel IPC debugging, and custom Resource Manager architecture on BlackBerry QNX Neutrino 8.0.
>
> To make sure we're the right fit for your timeline, could you briefly share:
> 1. Which QNX SDP version and target architecture (x86_64 or aarch64) are you deploying to?
> 2. Are you experiencing a specific real-time blocker (e.g. deadline jitter, thread stalls in `REPLY-blocked`, or driver architecture)?
> 3. Do you have a mutual NDA you prefer to use, or should I send over our standard mutual NDA?
>
> We typically run these engagements as fixed-scope sprints ($2,950 for Jitter/Latency Audits, $5,800 for Resource Manager Architecture) so your team has guaranteed deliverables with zero procurement delays.
>
> You can also grab a quick 20-minute slot on my calendar here if you prefer a call: [Insert Cal.com Link]
>
> Best regards,  
> [Your Name]  
> Creator of `qnx-top`  
> [Your Phone / LinkedIn]

---

### Template B: Proposal & Invoice (Closing the Deal)
**Subject:** `Proposal & SOW: QNX 8.0 Real-Time Latency Audit for [Company Name]`

> Hi [Name],
>
> Great speaking with you today. As discussed, here is the summary of the **Real-Time Jitter & IPC Audit** for [Company Name]:
>
> **Scope & Deliverables:**
> 1. Full IPC Channel and Message-Passing analysis on your [Vehicle / Robot Controller] subsystems.
> 2. Identification of thread blocking states (`REPLY-blocked` / priority inversion bottlenecks).
> 3. Formal Executive Remediation Report with targeted C/C++ code diffs to restore deterministic < 50µs timing guarantees.
> 4. 1-hour debrief video call with your engineering team.
>
> **Commercial Terms:**
> * **Fee:** $2,950 USD (Fixed scope).
> * **Turnaround:** 4 business days upon receiving repository access / QEMU test environment.
> * **Payment:** 50% deposit to initiate, 50% upon final report delivery.
> 
> Attached is the mutual NDA and formal Scope of Work (SOW). You can complete the deposit via card or ACH through this secure Stripe link: [Insert Stripe Payment Link].
>
> Looking forward to working with your team.
>
> Best regards,  
> [Your Name]

---

## 3. How to Set Up Invoicing as a Solo Developer

1. **Stripe Invoicing (Easiest for Credit Card & ACH):**
   - Create a free Stripe account.
   - Go to **Invoices** $\rightarrow$ **Create an Invoice**.
   - Enter their company email, set amount to $2,950 (or 50% deposit: $1,475), and enable Credit Card + ACH / Bank Transfer.
2. **International Bank Wire Transfer:**
   - If working with automotive firms in Germany, Japan, or Canada, they often prefer Direct Wire (SWIFT / IBAN).
   - Use **Wise Business** or your local business checking account to receive multi-currency transfers with near-zero FX fees.
3. **Tax Documents:**
   - If they are a US company and you are a US resident: provide **Form W-9**.
   - If you are outside the US: provide **Form W-8BEN-E** (available as a standard 1-page PDF).
