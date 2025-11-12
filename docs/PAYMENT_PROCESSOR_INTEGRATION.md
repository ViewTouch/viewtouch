# Payment Processor Integration Plan

## Status: Future Planning / Idea Documentation

**⚠️ Important Note**: This document represents **future planning and conceptual ideas** for payment processor integration. This is **NOT** a confirmed implementation plan and is subject to change based on future requirements, technical feasibility, and business decisions. This documentation serves as a reference for potential future development work.

## Overview

This document outlines potential plans and ideas for integrating modern payment processing with ViewTouch POS system, specifically designed for Android devices with SoftPOS (Tap to Pay) capability and thermal receipt printers. The content herein represents conceptual planning and should be reviewed and validated before any implementation begins.

## Proposed Payment Processor: Stripe Terminal

### Recommendation Rationale

**Stripe Terminal** is being considered as a potential payment processor option for the following reasons:

1. **Android SoftPOS Support**: Native Android SDK with Tap to Pay (contactless) support
2. **REST API Integration**: Modern HTTPS-based APIs (replaces legacy socket-based protocols)
3. **Restaurant Features**: Built-in support for tips, refunds, voids, split checks, and offline mode
4. **Developer-Friendly**: Comprehensive documentation, SDKs, and developer support
5. **Cost Structure**: 2.9% + 30¢ per transaction, no monthly fees or hardware costs
6. **PCI Compliance**: Reduced PCI scope through tokenization and secure payment handling
7. **Printer Integration**: Receipt data available via API for thermal printer integration

### Alternative Options Considered

- **Adyen Terminal API**: Good for multi-currency, but more complex integration
- **Shift4 Payments**: Restaurant-focused, but less Android SoftPOS documentation
- **Worldpay (FIS)**: Enterprise option, but longer certification timelines

## Architecture

### System Overview

```
┌─────────────────────────────────────────┐
│   ViewTouch (Linux Server)              │
│   - Order Management                    │
│   - HTTP/WebSocket Server (new)         │
│   - Stripe Processor Adapter (new)      │
│   - Payment State Management            │
└──────────────┬──────────────────────────┘
               │ WebSocket/HTTP
               │ {"order_id": 123, "amount": 42.50, "table": 5}
               │
┌──────────────▼──────────────────────────┐
│   Android Device (each table/server)    │
│   ┌──────────────────────────────────┐  │
│   │ Android Companion App            │  │
│   │ 1. Stripe Terminal SDK           │  │
│   │ 2. Payment Processing (SoftPOS)  │  │
│   │ 3. Multzo H10 Printer Control    │  │
│   │ 4. ViewTouch Communication       │  │
│   └──────────────────────────────────┘  │
│   ┌──────────────────────────────────┐  │
│   │ XSDL/X11 Display (ViewTouch UI)  │  │
│   └──────────────────────────────────┘  │
│   ┌──────────────────────────────────┐  │
│   │ Multzo H10 Printer (Bluetooth)   │  │
│   └──────────────────────────────────┘  │
└──────────────┬──────────────────────────┘
               │ HTTPS API
               │
┌──────────────▼──────────────────────────┐
│   Stripe API (Cloud)                    │
│   - Payment Processing                  │
│   - Settlement                          │
│   - Reporting                           │
└─────────────────────────────────────────┘
```

### Component Responsibilities

#### ViewTouch Server (Linux)
- Manages orders and payment requests
- Communicates with Android devices via WebSocket/HTTP
- Stores payment transaction records
- Handles payment state synchronization
- Processes refunds and voids from server-side

#### Android Companion App
- Receives payment requests from ViewTouch
- Processes payments using Stripe Terminal SDK (SoftPOS)
- Handles card reader interactions (NFC contactless, optional USB EMV)
- Controls Multzo H10 printer for receipt printing
- Sends payment confirmations back to ViewTouch
- Manages offline payment queue (store-and-forward)

#### Multzo H10 Printer
- Receives receipt data from Android app
- Prints customer receipts via Bluetooth/USB
- Uses standard ESC/POS commands
- Battery-powered, portable for table-side printing

## Hardware Requirements

### Android Devices
- **Operating System**: Android 10+ (API level 29+)
- **NFC**: Required for Tap to Pay (contactless payments)
- **Bluetooth**: Required for Multzo H10 printer connection
- **USB OTG** (optional): For USB card readers (EMV chip support)
- **Network**: Wi-Fi or cellular for internet connectivity
- **RAM**: Minimum 2GB recommended
- **Storage**: Sufficient space for app and offline transaction queue

### Receipt Printer
- **Model**: Multzo H10 Handheld Receipt Printer
- **Connection**: Bluetooth or USB
- **Protocol**: ESC/POS commands
- **Paper**: 58mm thermal paper
- **Battery**: Built-in rechargeable battery
- **Amazon Link**: https://www.amazon.com/Multzo-H10-Receipt-Handheld-barcodes/dp/B0CZLZZ8YF

### Server Requirements
- **OS**: Linux (existing ViewTouch server)
- **Network**: Internet connectivity for Stripe API calls
- **WebSocket Support**: For real-time device communication
- **HTTPS**: For secure API communication with Stripe

## Implementation Steps

### Phase 1: Stripe Account Setup
1. Create Stripe Terminal account
2. Obtain API keys (test mode initially)
3. Set up webhook endpoints for payment events
4. Configure merchant account settings
5. Test API connectivity

### Phase 2: ViewTouch Backend Changes

#### 2.1 Add Stripe Processor Type
- Add `CCAUTH_STRIPE = 4` to `main/data/credit.hh`
- Update `CCAUTH_MAX` to 4
- Add Stripe to authorization method settings

#### 2.2 Create Stripe Processor Adapter
- Create new class `StripeCard` (similar to `CCard`)
- Implement REST API methods:
  - `Sale()` - Process payment
  - `PreAuth()` - Pre-authorize payment
  - `FinishAuth()` - Complete pre-authorization
  - `Void()` - Void transaction
  - `Refund()` - Process refund
- Use HTTPS/REST instead of socket connections
- Handle Stripe API responses and errors

#### 2.3 Implement HTTP/WebSocket Server
- Add WebSocket server for Android device communication
- Define message protocol (JSON)
- Handle payment requests from Android devices
- Send payment confirmations to ViewTouch POS
- Manage device registration and authentication

#### 2.4 Update Payment Flow
- Modify `term_view.cc` to support Stripe processor
- Update `Terminal::EndDay()` for Stripe settlement
- Add Stripe-specific payment dialogs
- Update payment zone to work with Android devices

### Phase 3: Android Companion App

#### 3.1 Stripe Terminal SDK Integration
- Add Stripe Terminal SDK to Android project
- Initialize Stripe Terminal with API keys
- Implement payment collection flow
- Handle payment method selection (contactless, chip, swipe)
- Process payment confirmations

#### 3.2 Multzo H10 Printer Integration
- Implement Bluetooth/USB printer connection
- Create ESC/POS command generator
- Format receipt data from Stripe transaction
- Handle printer errors and retries
- Support receipt formatting (items, totals, tips)

#### 3.3 ViewTouch Communication
- Implement WebSocket client for ViewTouch server
- Send payment requests to server
- Receive payment confirmations
- Handle offline mode (queue transactions)
- Implement device registration and authentication

#### 3.4 UI Components
- Create payment overlay UI (over XSDL display)
- Show payment progress and status
- Display receipt preview
- Handle payment errors and retries
- Support tip adjustment

### Phase 4: Testing and Validation

#### 4.1 Unit Testing
- Test Stripe API integration
- Test printer communication
- Test WebSocket communication
- Test offline mode functionality

#### 4.2 Integration Testing
- End-to-end payment flow
- Multi-device payment processing
- Offline payment queue
- Receipt printing
- Error handling and retries

#### 4.3 User Acceptance Testing
- Test with real Android devices
- Test with Multzo H10 printer
- Test in restaurant environment
- Validate payment processing accuracy
- Test tip adjustments and refunds

## Communication Protocol

### Message Format
All messages use JSON format over WebSocket or HTTP.

### Payment Request (ViewTouch → Android Device)
```json
{
  "action": "payment",
  "order_id": 123,
  "amount": 4250,
  "currency": "usd",
  "tip": 500,
  "table": 5,
  "server": "John",
  "items": [
    {"name": "Burger", "price": 1200},
    {"name": "Fries", "price": 500}
  ],
  "timestamp": "2025-01-27T10:30:00Z"
}
```

### Payment Response (Android Device → ViewTouch)
```json
{
  "status": "approved",
  "order_id": 123,
  "transaction_id": "tx_abc123",
  "payment_intent_id": "pi_xyz789",
  "card_last4": "4242",
  "card_brand": "visa",
  "amount": 4250,
  "tip": 500,
  "total": 4750,
  "receipt_url": "https://pay.stripe.com/receipts/...",
  "timestamp": "2025-01-27T10:31:00Z",
  "device_id": "android_device_001"
}
```

### Error Response
```json
{
  "status": "declined",
  "order_id": 123,
  "error_code": "card_declined",
  "error_message": "Your card was declined.",
  "timestamp": "2025-01-27T10:31:00Z",
  "device_id": "android_device_001"
}
```

### Refund Request (ViewTouch → Android Device)
```json
{
  "action": "refund",
  "order_id": 123,
  "transaction_id": "tx_abc123",
  "amount": 4250,
  "reason": "customer_request",
  "timestamp": "2025-01-27T11:00:00Z"
}
```

## Android App Requirements

### Dependencies
- Stripe Terminal SDK: `com.stripe:stripeterminal:3.x.x`
- WebSocket Client: `okhttp3` or similar
- Bluetooth Printer: ESC/POS library
- JSON Parsing: `org.json` or Gson

### Permissions
- `INTERNET` - For Stripe API and ViewTouch communication
- `BLUETOOTH` - For Multzo H10 printer
- `BLUETOOTH_ADMIN` - For printer pairing
- `NFC` - For Tap to Pay
- `ACCESS_NETWORK_STATE` - For connectivity checks

### Key Features
1. **Payment Processing**
   - Stripe Terminal SDK integration
   - Contactless payment (NFC)
   - EMV chip card support (USB reader)
   - Magnetic stripe support (USB reader)

2. **Printer Integration**
   - Bluetooth printer connection
   - ESC/POS command generation
   - Receipt formatting
   - Error handling and retries

3. **Communication**
   - WebSocket client for ViewTouch
   - Offline transaction queue
   - Automatic reconnection
   - Device authentication

4. **UI Components**
   - Payment overlay (over XSDL display)
   - Payment progress indicator
   - Receipt preview
   - Error messages
   - Tip adjustment interface

## ViewTouch Backend Changes

### New Files
- `src/core/stripe_card.hh` - Stripe processor adapter header
- `src/core/stripe_card.cc` - Stripe processor adapter implementation
- `src/network/device_server.hh` - WebSocket server for Android devices
- `src/network/device_server.cc` - WebSocket server implementation

### Modified Files
- `main/data/credit.hh` - Add `CCAUTH_STRIPE` processor type
- `main/data/settings.cc` - Add Stripe to authorization methods
- `term/term_view.cc` - Add Stripe payment handling
- `main/hardware/terminal.cc` - Update EndDay() for Stripe settlement
- `zone/payment_zone.cc` - Add Stripe payment dialogs

### New Dependencies
- HTTP client library (e.g., `libcurl` or `cpp-httplib`)
- JSON library (existing `nlohmann/json` or similar)
- WebSocket server library (e.g., `websocketpp` or custom implementation)
- SSL/TLS library (for HTTPS)

## Testing Plan

### Unit Tests
- Stripe API integration tests
- Printer communication tests
- WebSocket communication tests
- Payment state management tests
- Error handling tests

### Integration Tests
- End-to-end payment flow
- Multi-device payment processing
- Offline payment queue
- Receipt printing
- Refund processing
- Tip adjustments

### User Acceptance Tests
- Real Android device testing
- Multzo H10 printer testing
- Restaurant environment testing
- Payment processing accuracy
- Error recovery
- Offline mode functionality

## Security Considerations

### PCI Compliance
- Never store full card numbers
- Use Stripe tokens for payment processing
- Implement secure communication (HTTPS, WSS)
- Follow PCI DSS guidelines for POS systems
- Regular security audits

### Device Security
- Authenticate Android devices
- Encrypt device communication
- Implement device registration
- Monitor for unauthorized devices
- Secure API key storage

### Data Protection
- Encrypt sensitive data at rest
- Use secure communication protocols
- Implement access controls
- Regular backup and recovery
- Audit logging

## Future Enhancements

### Phase 2 Features
- Support for multiple payment processors
- Gift card processing
- Loyalty program integration
- Multi-currency support
- Advanced reporting and analytics

### Phase 3 Features
- Mobile ordering integration
- Online payment processing
- Delivery management
- Customer management system
- Marketing and promotions

## References

### Stripe Terminal Documentation
- Android SDK: https://stripe.com/docs/terminal
- API Reference: https://stripe.com/docs/api/terminal
- Testing: https://stripe.com/docs/terminal/testing

### Multzo H10 Printer
- Product: https://www.amazon.com/Multzo-H10-Receipt-Handheld-barcodes/dp/B0CZLZZ8YF
- ESC/POS Commands: Standard thermal printer protocol
- Bluetooth: Standard Bluetooth Serial Port Profile (SPP)

### ViewTouch Documentation
- Development Workflow: `docs/DEVELOPMENT.md`
- Payment Processing: `main/data/credit.hh`
- Terminal Communication: `term/term_view.cc`

## Implementation Checklist

### Backend Development
- [ ] Add `CCAUTH_STRIPE` processor type
- [ ] Create `StripeCard` class
- [ ] Implement Stripe REST API integration
- [ ] Create WebSocket server for Android devices
- [ ] Update payment flow for Stripe
- [ ] Add Stripe settings to configuration
- [ ] Implement device authentication
- [ ] Add error handling and logging
- [ ] Update EndDay() for Stripe settlement
- [ ] Add unit tests

### Android App Development
- [ ] Set up Android project
- [ ] Integrate Stripe Terminal SDK
- [ ] Implement payment processing
- [ ] Integrate Multzo H10 printer
- [ ] Implement WebSocket client
- [ ] Create payment UI components
- [ ] Implement offline mode
- [ ] Add device registration
- [ ] Add error handling
- [ ] Add unit tests

### Testing
- [ ] Unit tests for backend
- [ ] Unit tests for Android app
- [ ] Integration tests
- [ ] End-to-end testing
- [ ] User acceptance testing
- [ ] Security testing
- [ ] Performance testing
- [ ] Offline mode testing

### Deployment
- [ ] Stripe account setup
- [ ] API key configuration
- [ ] Device registration system
- [ ] Production testing
- [ ] Documentation
- [ ] Training materials
- [ ] Rollout plan

## Notes

- This integration replaces the legacy socket-based payment processors (CreditCheq, MainStreet)
- The new system uses modern REST APIs and WebSocket communication
- Android devices act as payment terminals with SoftPOS capability
- Multzo H10 printer provides portable receipt printing
- Offline mode ensures payments can be processed even without internet connectivity
- Regular updates and maintenance will be required for Stripe SDK and Android app

## Revision History

- **2025-01-27**: Initial document created as future planning/idea documentation
- Future revisions will be documented here

## Disclaimer

This document is provided for planning and reference purposes only. Any implementation decisions should be made after:
- Thorough technical feasibility analysis
- Business requirements review
- Cost-benefit analysis
- Security and compliance review
- Stakeholder approval

No implementation should begin without proper validation and approval of the proposed approach.

