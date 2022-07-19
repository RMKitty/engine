// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_COMMON_ACCESSIBILITY_BRIDGE_H_
#define FLUTTER_SHELL_PLATFORM_COMMON_ACCESSIBILITY_BRIDGE_H_

#include <unordered_map>

#include "flutter/fml/mapping.h"
#include "flutter/shell/platform/embedder/embedder.h"

#include "flutter/third_party/accessibility/ax/ax_event_generator.h"
#include "flutter/third_party/accessibility/ax/ax_tree.h"
#include "flutter/third_party/accessibility/ax/ax_tree_observer.h"
#include "flutter/third_party/accessibility/ax/platform/ax_platform_node_delegate.h"

#include "flutter_platform_node_delegate.h"

namespace flutter {

//------------------------------------------------------------------------------
/// Use this class to maintain an accessibility tree. This class consumes
/// semantics updates from the embedder API and produces an accessibility tree
/// in the native format.
///
/// The bridge creates an AXTree to hold the semantics data that comes from
/// Flutter semantics updates. The tree holds AXNode[s] which contain the
/// semantics information for semantics node. The AXTree resemble the Flutter
/// semantics tree in the Flutter framework. The bridge also uses
/// FlutterPlatformNodeDelegate to wrap each AXNode in order to provide
/// an accessibility tree in the native format.
///
/// This class takes in a AccessibilityBridgeDelegate instance and is in charge
/// of its lifecycle. The delegate are used to handle the accessibility events
/// and actions.
///
/// To use this class, you must provide your own implementation of
/// FlutterPlatformNodeDelegate and AccessibilityBridgeDelegate.
class AccessibilityBridge
    : public std::enable_shared_from_this<AccessibilityBridge>,
      public FlutterPlatformNodeDelegate::OwnerBridge,
      private ui::AXTreeObserver {
 public:
  //-----------------------------------------------------------------------------
  /// Delegate to handle requests from the accessibility bridge. The requests
  /// include sending accessibility event to native accessibility system,
  /// routing accessibility action to the Flutter framework, and creating
  /// platform specific FlutterPlatformNodeDelegate.
  ///
  /// The accessibility events are generated when accessibility tree changes.
  /// These events must be sent to the native accessibility system through
  /// the native API for the system to pick up the changes
  /// (e.g. NSAccessibilityPostNotification in MacOS).
  ///
  /// The accessibility actions are generated by the native accessibility system
  /// when users interacted with the assistive technologies. Those actions
  /// needed to be sent to the Flutter framework.
  ///
  /// Each platform needs to implement the FlutterPlatformNodeDelegate and
  /// returns its platform specific instance of FlutterPlatformNodeDelegate
  /// in this delegate.
  class AccessibilityBridgeDelegate {
   public:
    virtual ~AccessibilityBridgeDelegate() = default;
    //---------------------------------------------------------------------------
    /// @brief      Handle accessibility events generated due to accessibility
    ///             tree changes. These events are generated in accessibility
    ///             bridge and needed to be sent to native accessibility system.
    ///             See ui::AXEventGenerator::Event for possible events.
    ///
    /// @param[in]  targeted_event      The object that contains both the
    ///                                 generated event and the event target.
    virtual void OnAccessibilityEvent(
        ui::AXEventGenerator::TargetedEvent targeted_event) = 0;

    //---------------------------------------------------------------------------
    /// @brief      Dispatch accessibility action back to the Flutter framework.
    ///             These actions are generated in the native accessibility
    ///             system when users interact with the assistive technologies.
    ///             For example, a
    ///             FlutterSemanticsAction::kFlutterSemanticsActionTap is
    ///             fired when user click or touch the screen.
    ///
    /// @param[in]  target              The semantics node id of the action
    ///                                 target.
    /// @param[in]  action              The generated flutter semantics action.
    /// @param[in]  data                Additional data associated with the
    ///                                 action.
    virtual void DispatchAccessibilityAction(AccessibilityNodeId target,
                                             FlutterSemanticsAction action,
                                             fml::MallocMapping data) = 0;

    //---------------------------------------------------------------------------
    /// @brief      Creates a platform specific FlutterPlatformNodeDelegate.
    ///             Ownership passes to the caller. This method will be called
    ///             by accessibility bridge whenever a new AXNode is created in
    ///             AXTree. Each platform needs to implement this method in
    ///             order to inject its subclass into the accessibility bridge.
    virtual std::shared_ptr<FlutterPlatformNodeDelegate>
    CreateFlutterPlatformNodeDelegate() = 0;
  };

  //-----------------------------------------------------------------------------
  /// @brief      Creates a new instance of a accessibility bridge.
  ///
  /// @param[in]  user_data           A custom pointer to the data of your
  ///                                 choice. This pointer can be retrieve later
  ///                                 through GetUserData().
  explicit AccessibilityBridge(
      std::unique_ptr<AccessibilityBridgeDelegate> delegate);
  ~AccessibilityBridge();

  //-----------------------------------------------------------------------------
  /// @brief      The ID of the root node in the accessibility tree. In Flutter,
  //              this is always 0.
  static constexpr int32_t kRootNodeId = 0;

  //------------------------------------------------------------------------------
  /// @brief      Adds a semantics node update to the pending semantics update.
  ///             Calling this method alone will NOT update the semantics tree.
  ///             To flush the pending updates, call the CommitUpdates().
  ///
  /// @param[in]  node           A pointer to the semantics node update.
  void AddFlutterSemanticsNodeUpdate(const FlutterSemanticsNode* node);

  //------------------------------------------------------------------------------
  /// @brief      Adds a custom semantics action update to the pending semantics
  ///             update. Calling this method alone will NOT update the
  ///             semantics tree. To flush the pending updates, call the
  ///             CommitUpdates().
  ///
  /// @param[in]  action           A pointer to the custom semantics action
  ///                              update.
  void AddFlutterSemanticsCustomActionUpdate(
      const FlutterSemanticsCustomAction* action);

  //------------------------------------------------------------------------------
  /// @brief      Flushes the pending updates and applies them to this
  ///             accessibility bridge. Calling this with no pending updates
  ///             does nothing, and callers should call this method at the end
  ///             of an atomic batch to avoid leaving the tree in a unstable
  ///             state. For example if a node reparents from A to B, callers
  ///             should only call this method when both removal from A and
  ///             addition to B are in the pending updates.
  void CommitUpdates();

  //------------------------------------------------------------------------------
  /// @brief      Get the flutter platform node delegate with the given id from
  ///             this accessibility bridge. Returns expired weak_ptr if the
  ///             delegate associated with the id does not exist or has been
  ///             removed from the accessibility tree.
  ///
  /// @param[in]  id           The id of the flutter accessibility node you want
  ///                          to retrieve.
  std::weak_ptr<FlutterPlatformNodeDelegate>
  GetFlutterPlatformNodeDelegateFromID(AccessibilityNodeId id) const;

  //------------------------------------------------------------------------------
  /// @brief      Get the ax tree data from this accessibility bridge. The tree
  ///             data contains information such as the id of the node that
  ///             has the keyboard focus or the text selection range.
  const ui::AXTreeData& GetAXTreeData() const;

  //------------------------------------------------------------------------------
  /// @brief      Gets all pending accessibility events generated during
  ///             semantics updates. This is useful when deciding how to handle
  ///             events in AccessibilityBridgeDelegate::OnAccessibilityEvent in
  ///             case one may decide to handle an event differently based on
  ///             all pending events.
  const std::vector<ui::AXEventGenerator::TargetedEvent> GetPendingEvents();

  //------------------------------------------------------------------------------
  /// @brief      Update the AccessibilityBridgeDelegate stored in the
  ///             accessibility bridge to a new one.
  void UpdateDelegate(std::unique_ptr<AccessibilityBridgeDelegate> delegate);

 private:
  // See FlutterSemanticsNode in embedder.h
  typedef struct {
    int32_t id;
    FlutterSemanticsFlag flags;
    FlutterSemanticsAction actions;
    int32_t text_selection_base;
    int32_t text_selection_extent;
    int32_t scroll_child_count;
    int32_t scroll_index;
    double scroll_position;
    double scroll_extent_max;
    double scroll_extent_min;
    double elevation;
    double thickness;
    std::string label;
    std::string hint;
    std::string value;
    std::string increased_value;
    std::string decreased_value;
    FlutterTextDirection text_direction;
    FlutterRect rect;
    FlutterTransformation transform;
    std::vector<int32_t> children_in_traversal_order;
    std::vector<int32_t> custom_accessibility_actions;
  } SemanticsNode;

  // See FlutterSemanticsCustomAction in embedder.h
  typedef struct {
    int32_t id;
    FlutterSemanticsAction override_action;
    std::string label;
    std::string hint;
  } SemanticsCustomAction;

  std::unordered_map<AccessibilityNodeId,
                     std::shared_ptr<FlutterPlatformNodeDelegate>>
      id_wrapper_map_;
  ui::AXTree tree_;
  ui::AXEventGenerator event_generator_;
  std::unordered_map<int32_t, SemanticsNode> pending_semantics_node_updates_;
  std::unordered_map<int32_t, SemanticsCustomAction>
      pending_semantics_custom_action_updates_;
  AccessibilityNodeId last_focused_id_ = ui::AXNode::kInvalidAXID;
  std::unique_ptr<AccessibilityBridgeDelegate> delegate_;

  void InitAXTree(const ui::AXTreeUpdate& initial_state);
  void GetSubTreeList(SemanticsNode target, std::vector<SemanticsNode>& result);
  void ConvertFluterUpdate(const SemanticsNode& node,
                           ui::AXTreeUpdate& tree_update);
  void SetRoleFromFlutterUpdate(ui::AXNodeData& node_data,
                                const SemanticsNode& node);
  void SetStateFromFlutterUpdate(ui::AXNodeData& node_data,
                                 const SemanticsNode& node);
  void SetActionsFromFlutterUpdate(ui::AXNodeData& node_data,
                                   const SemanticsNode& node);
  void SetBooleanAttributesFromFlutterUpdate(ui::AXNodeData& node_data,
                                             const SemanticsNode& node);
  void SetIntAttributesFromFlutterUpdate(ui::AXNodeData& node_data,
                                         const SemanticsNode& node);
  void SetIntListAttributesFromFlutterUpdate(ui::AXNodeData& node_data,
                                             const SemanticsNode& node);
  void SetStringListAttributesFromFlutterUpdate(ui::AXNodeData& node_data,
                                                const SemanticsNode& node);
  void SetNameFromFlutterUpdate(ui::AXNodeData& node_data,
                                const SemanticsNode& node);
  void SetValueFromFlutterUpdate(ui::AXNodeData& node_data,
                                 const SemanticsNode& node);
  void SetTreeData(const SemanticsNode& node, ui::AXTreeUpdate& tree_update);
  SemanticsNode FromFlutterSemanticsNode(
      const FlutterSemanticsNode* flutter_node);
  SemanticsCustomAction FromFlutterSemanticsCustomAction(
      const FlutterSemanticsCustomAction* flutter_custom_action);

  // |AXTreeObserver|
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;

  // |AXTreeObserver|
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;

  // |AXTreeObserver|
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;

  // |AXTreeObserver|
  void OnNodeDeleted(ui::AXTree* tree, AccessibilityNodeId node_id) override;

  // |AXTreeObserver|
  void OnNodeReparented(ui::AXTree* tree, ui::AXNode* node) override;

  // |AXTreeObserver|
  void OnRoleChanged(ui::AXTree* tree,
                     ui::AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override;

  // |AXTreeObserver|
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  // |FlutterPlatformNodeDelegate::OwnerBridge|
  void SetLastFocusedId(AccessibilityNodeId node_id) override;

  // |FlutterPlatformNodeDelegate::OwnerBridge|
  AccessibilityNodeId GetLastFocusedId() override;

  // |FlutterPlatformNodeDelegate::OwnerBridge|
  gfx::NativeViewAccessible GetNativeAccessibleFromId(
      AccessibilityNodeId id) override;

  // |FlutterPlatformNodeDelegate::OwnerBridge|
  void DispatchAccessibilityAction(AccessibilityNodeId target,
                                   FlutterSemanticsAction action,
                                   fml::MallocMapping data) override;

  // |FlutterPlatformNodeDelegate::OwnerBridge|
  gfx::RectF RelativeToGlobalBounds(const ui::AXNode* node,
                                    bool& offscreen,
                                    bool clip_bounds) override;

  BASE_DISALLOW_COPY_AND_ASSIGN(AccessibilityBridge);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_COMMON_ACCESSIBILITY_BRIDGE_H_
