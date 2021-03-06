<?php

/**
 * @file
 * Admin page callback file for the user module.
 */

/**
 * Form builder; Cancel multiple accounts at the same time.
 *
 * @ingroup forms
 * @see user_cancel_user_action()
 */
function user_multiple_cancel_confirm($form, &$form_state) {
  if (isset($_SESSION['user_cancel_action']['timestamp']) && (REQUEST_TIME - $_SESSION['user_cancel_action']['timestamp'] < 6000)) {
    $uids = $_SESSION['user_cancel_action']['uids'];
  }
  else {
    $uids = array();
  }

  $accounts = user_load_multiple($uids);

  // Output a notice that user 1 cannot be canceled.
  if (isset($accounts[1])) {
    $redirect = (count($accounts) == 1);
    $message = t('The user account %name cannot be cancelled.', array('%name' => $accounts[1]->name));
    backdrop_set_message($message, $redirect ? 'error' : 'warning');
    unset($accounts[1]);
    // If only user 1 was selected, redirect to the overview.
    if ($redirect) {
      backdrop_goto('admin/people');
    }
  }

  if (empty($accounts)) {
    $destination = isset($_GET['destination']) ? $_GET['destination'] : 'admin/people';
    $form['empty']['#markup'] = '<p>' . t('Return to the <a href="!url">content administration page</a>.', array('!url' => url($destination))) . '</p>';
    backdrop_set_message(t('No user accounts have been selected for cancellation.'), 'error');
    return $form;
  }

  $form_state['accounts'] = $accounts;

  $form['user_list'] = array(
    '#theme' => 'item_list',
    '#items' => array(),
  );
  foreach ($accounts as $account) {
    $form['user_list']['#items'][] = check_plain($account->name);
  }

  module_load_include('inc', 'user', 'user.pages');
  $form['user_cancel_method'] = array(
    '#type' => 'item',
    '#title' => t('When cancelling these accounts'),
  );
  $form['user_cancel_method'] += user_cancel_methods();

  // Remove method descriptions.
  foreach (element_children($form['user_cancel_method']) as $element) {
    unset($form['user_cancel_method'][$element]['#description']);
  }

  // Allow to send the account cancellation confirmation mail.
  $form['user_cancel_confirm'] = array(
    '#type' => 'checkbox',
    '#title' => t('Require e-mail confirmation to cancel account.'),
    '#default_value' => FALSE,
    '#description' => t('When enabled, the user must confirm the account cancellation via e-mail.'),
  );
  // Also allow to send account canceled notification mail, if enabled.
  $form['user_cancel_notify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify user when account is canceled.'),
    '#default_value' => FALSE,
    '#access' => config_get('system.core', 'user_mail_status_canceled_notify'),
    '#description' => t('When enabled, the user will receive an e-mail notification after the account has been cancelled.'),
  );

  $confirm_question = format_plural(count($accounts), 'Are you sure you want to cancel this user account?', 'Are you sure you want to cancel these user accounts?');
  return confirm_form($form, $confirm_question, 'admin/people', NULL, t('Cancel accounts'), t('Cancel'));
}

/**
 * Submit handler for mass-account cancellation form.
 *
 * @see user_multiple_cancel_confirm()
 * @see user_cancel_confirm_form_submit()
 */
function user_multiple_cancel_confirm_submit($form, &$form_state) {
  global $user;

  foreach ($form_state['accounts'] as $uid => $account) {
    // Prevent programmatic form submissions from cancelling user 1.
    if ($uid <= 1) {
      continue;
    }
    // Prevent user administrators from deleting themselves without confirmation.
    if ($uid == $user->uid) {
      $admin_form_state = $form_state;
      unset($admin_form_state['values']['user_cancel_confirm']);
      // The $user global is not a complete user entity, so load the full
      // entity.
      $admin_form_state['values']['_account'] = user_load($user->uid);
      user_cancel_confirm_form_submit(array(), $admin_form_state);
    }
    else {
      user_cancel($form_state['values'], $uid, $form_state['values']['user_cancel_method']);
    }
  }

  $form_state['redirect'] = 'admin/people';
}

/**
 * Form builder; Configure user settings for this site.
 *
 * @ingroup forms
 */
function user_admin_settings($form, &$form_state) {
  $config = config('system.core');
  $mail_config = config('user.mail');

  // Settings for anonymous users.
  $form['anonymous_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Anonymous users'),
  );
  $form['anonymous_settings']['anonymous'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#default_value' => $config->get('anonymous'),
    '#description' => t('The name used to indicate anonymous users.'),
    '#required' => TRUE,
  );

  // Administrative role option.
  $form['admin_role'] = array(
    '#type' => 'fieldset',
    '#title' => t('Administrator role'),
  );

  // Do not allow users to set the anonymous or authenticated user roles as the
  // administrator role.
  $roles = user_roles();
  unset($roles[BACKDROP_ANONYMOUS_ROLE]);
  unset($roles[BACKDROP_AUTHENTICATED_ROLE]);
  $roles[0] = t('disabled');

  $form['admin_role']['user_admin_role'] = array(
    '#type' => 'select',
    '#title' => t('Administrator role'),
    '#default_value' => $config->get('user_admin_role'),
    '#options' => $roles,
    '#description' => t('This role will be automatically assigned new permissions whenever a module is enabled. Changing this setting will not affect existing permissions.'),
  );

  // User registration settings.
  $form['registration_cancellation'] = array(
    '#type' => 'fieldset',
    '#title' => t('Registration and cancellation'),
  );

  $form['registration_cancellation']['user_register'] = array(
    '#type' => 'radios',
    '#title' => t('Who can register accounts?'),
    '#default_value' => $config->get('user_register'),
    '#options' => array(
      USER_REGISTER_ADMINISTRATORS_ONLY => t('Administrators only'),
      USER_REGISTER_VISITORS => t('Visitors'),
      USER_REGISTER_VISITORS_ADMINISTRATIVE_APPROVAL => t('Visitors, but administrator approval is required'),
    )
  );
  $form['registration_cancellation']['user_email_verification'] = array(
    '#type' => 'checkbox',
    '#title' => t('Require e-mail verification when a visitor creates an account.'),
    '#default_value' => $config->get('user_email_verification'),
    '#description' => t('New users will be required to validate their e-mail address prior to logging into the site, and will be assigned a system-generated password. With this setting disabled, users will be logged in immediately upon registering, and may select their own passwords during registration.')
  );
  module_load_include('inc', 'user', 'user.pages');
  $form['registration_cancellation']['user_cancel_method'] = array(
    '#type' => 'item',
    '#title' => t('When cancelling a user account'),
    '#description' => t('Users with the %select-cancel-method or %administer-users <a href="@permissions-url">permissions</a> can override this default method.', array('%select-cancel-method' => t('Select method for cancelling account'), '%administer-users' => t('Administer users'), '@permissions-url' => url('admin/config/people/permissions'))),
  );
  $form['registration_cancellation']['user_cancel_method'] += user_cancel_methods();
  foreach (element_children($form['registration_cancellation']['user_cancel_method']) as $element) {
    // Remove all account cancellation methods that have #access defined, as
    // those cannot be configured as default method.
    if (isset($form['registration_cancellation']['user_cancel_method'][$element]['#access'])) {
      $form['registration_cancellation']['user_cancel_method'][$element]['#access'] = FALSE;
    }
    // Remove the description (only displayed on the confirmation form).
    else {
      unset($form['registration_cancellation']['user_cancel_method'][$element]['#description']);
    }
  }

  // Account settings.
  $form['personalization'] = array(
    '#type' => 'fieldset',
    '#title' => t('Personalization'),
  );
  $form['personalization']['user_signatures'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable signatures.'),
    '#default_value' => $config->get('user_signatures'),
  );
  // If picture support is enabled, check whether the picture directory exists.
  $config = config('system.core');
  if ($config->get('user_pictures')) {
    $picture_path =  file_default_scheme() . '://' . config_get('system.core', 'user_picture_path');
    if (!file_prepare_directory($picture_path, FILE_CREATE_DIRECTORY)) {
      form_set_error('user_picture_path', t('The directory %directory does not exist or is not writable.', array('%directory' => $picture_path)));
      watchdog('file system', 'The directory %directory does not exist or is not writable.', array('%directory' => $picture_path), WATCHDOG_ERROR);
    }
  }
  $picture_support = $config->get('user_pictures');
  $form['personalization']['user_pictures'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable user pictures.'),
    '#default_value' => $picture_support,
  );
  backdrop_add_js(backdrop_get_path('module', 'user') . '/user.js');
  $form['personalization']['pictures'] = array(
    '#type' => 'container',
    '#states' => array(
      // Hide the additional picture settings when user pictures are disabled.
      'invisible' => array(
        'input[name="user_pictures"]' => array('checked' => FALSE),
      ),
    ),
  );
  $form['personalization']['pictures']['user_picture_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Picture directory'),
    '#default_value' => $config->get('user_picture_path'),
    '#size' => 30,
    '#maxlength' => 255,
    '#description' => t('Subdirectory in the file upload directory where pictures will be stored.'),
  );
  $form['personalization']['pictures']['user_picture_default'] = array(
    '#type' => 'textfield',
    '#title' => t('Default picture'),
    '#default_value' => $config->get('user_picture_default'),
    '#size' => 30,
    '#maxlength' => 255,
    '#description' => t('URL of picture to display for users with no custom picture selected. Leave blank for none.'),
  );
  if (module_exists('image')) {
    $form['personalization']['pictures']['settings']['user_picture_style'] = array(
      '#type' => 'select',
      '#title' => t('Picture display style'),
      '#options' => image_style_options(TRUE, PASS_THROUGH),
      '#default_value' => $config->get('user_picture_style'),
      '#description' => t('The style selected will be used on display, while the original image is retained. Styles may be configured in the <a href="!url">Image styles</a> administration area.', array('!url' => url('admin/config/media/image-styles'))),
    );
  }
  $form['personalization']['pictures']['user_picture_dimensions'] = array(
    '#type' => 'textfield',
    '#title' => t('Picture upload dimensions'),
    '#default_value' => $config->get('user_picture_dimensions'),
    '#size' => 10,
    '#maxlength' => 10,
    '#field_suffix' => ' ' . t('pixels'),
    '#description' => t('Pictures larger than this will be scaled down to this size.'),
  );
  $form['personalization']['pictures']['user_picture_file_size'] = array(
    '#type' => 'number',
    '#title' => t('Picture upload file size'),
    '#default_value' => $config->get('user_picture_file_size'),
    '#min' => 0,
    '#max' => 1000000000,
    '#field_suffix' => ' ' . t('KB'),
    '#description' => t('Maximum allowed file size for uploaded pictures. Upload size is normally limited only by the PHP maximum post and file upload settings, and images are automatically scaled down to the dimensions specified above.'),
  );
  $form['personalization']['pictures']['user_picture_guidelines'] = array(
    '#type' => 'textarea',
    '#title' => t('Picture guidelines'),
    '#default_value' => $config->get('user_picture_guidelines'),
    '#description' => t("This text is displayed at the picture upload form in addition to the default guidelines. It's useful for helping or instructing your users."),
  );

  $form['email_title'] = array(
    '#type' => 'item',
    '#title' => t('E-mails'),
  );
  $form['email'] = array(
    '#type' => 'vertical_tabs',
  );
  // These email tokens are shared for all settings, so just define
  // the list once to help ensure they stay in sync.
  $email_token_help = t('Available variables are: [site:name], [site:url], [user:name], [user:mail], [site:login-url], [site:url-brief], [user:edit-url], [user:one-time-login-url], [user:cancel-url].');

  $form['email_admin_created'] = array(
    '#type' => 'fieldset',
    '#title' => t('Welcome (new user created by administrator)'),
    '#collapsible' => TRUE,
    '#collapsed' => ($config->get('user_register') != USER_REGISTER_ADMINISTRATORS_ONLY),
    '#description' => t('Edit the welcome e-mail messages sent to new member accounts created by an administrator.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_admin_created']['user_mail_register_admin_created_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('register_admin_created_subject'),
    '#maxlength' => 180,
  );
  $form['email_admin_created']['user_mail_register_admin_created_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('register_admin_created_body'),
    '#rows' => 15,
  );

  $form['email_pending_approval'] = array(
    '#type' => 'fieldset',
    '#title' => t('Welcome (awaiting approval)'),
    '#collapsible' => TRUE,
    '#collapsed' => ($config->get('user_register') != USER_REGISTER_VISITORS_ADMINISTRATIVE_APPROVAL),
    '#description' => t('Edit the welcome e-mail messages sent to new members upon registering, when administrative approval is required.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_pending_approval']['user_mail_register_pending_approval_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('register_pending_approval_subject'),
    '#maxlength' => 180,
  );
  $form['email_pending_approval']['user_mail_register_pending_approval_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('register_pending_approval_body'),
    '#rows' => 8,
  );

  $form['email_no_approval_required'] = array(
    '#type' => 'fieldset',
    '#title' => t('Welcome (no approval required)'),
    '#collapsible' => TRUE,
    '#collapsed' => ($config->get('user_register') != USER_REGISTER_VISITORS),
    '#description' => t('Edit the welcome e-mail messages sent to new members upon registering, when no administrator approval is required.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_no_approval_required']['user_mail_register_no_approval_required_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('register_no_approval_required_subject'),
    '#maxlength' => 180,
  );
  $form['email_no_approval_required']['user_mail_register_no_approval_required_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('register_no_approval_required_body'),
    '#rows' => 15,
  );

  $form['email_password_reset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Password recovery'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Edit the e-mail messages sent to users who request a new password.') . ' ' . $email_token_help,
    '#group' => 'email',
    '#weight' => 10,
  );
  $form['email_password_reset']['user_mail_password_reset_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('password_reset_subject'),
    '#maxlength' => 180,
  );
  $form['email_password_reset']['user_mail_password_reset_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('password_reset_body'),
    '#rows' => 12,
  );

  $form['email_activated'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account activation'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Enable and edit e-mail messages sent to users upon account activation (when an administrator activates an account of a user who has already registered, on a site where administrative approval is required).') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_activated']['user_mail_status_activated_notify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify user when account is activated.'),
    '#default_value' => $config->get('user_mail_status_activated_notify'),
  );
  $form['email_activated']['settings'] = array(
    '#type' => 'container',
    '#states' => array(
      // Hide the additional settings when this email is disabled.
      'invisible' => array(
        'input[name="user_mail_status_activated_notify"]' => array('checked' => FALSE),
      ),
    ),
  );
  $form['email_activated']['settings']['user_mail_status_activated_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('status_activated_subject'),
    '#maxlength' => 180,
  );
  $form['email_activated']['settings']['user_mail_status_activated_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('status_activated_body'),
    '#rows' => 15,
  );

  $form['email_blocked'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account blocked'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Enable and edit e-mail messages sent to users when their accounts are blocked.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_blocked']['user_mail_status_blocked_notify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify user when account is blocked.'),
    '#default_value' => $config->get('user_mail_status_blocked_notify'),
  );
  $form['email_blocked']['settings'] = array(
    '#type' => 'container',
    '#states' => array(
      // Hide the additional settings when the blocked email is disabled.
      'invisible' => array(
        'input[name="user_mail_status_blocked_notify"]' => array('checked' => FALSE),
      ),
    ),
  );
  $form['email_blocked']['settings']['user_mail_status_blocked_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('status_blocked_subject'),
    '#maxlength' => 180,
  );
  $form['email_blocked']['settings']['user_mail_status_blocked_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('status_blocked_body'),
    '#rows' => 3,
  );

  $form['email_cancel_confirm'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account cancellation confirmation'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Edit the e-mail messages sent to users when they attempt to cancel their accounts.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_cancel_confirm']['user_mail_cancel_confirm_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('cancel_confirm_subject'),
    '#maxlength' => 180,
  );
  $form['email_cancel_confirm']['user_mail_cancel_confirm_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('cancel_confirm_body'),
    '#rows' => 3,
  );

  $form['email_canceled'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account canceled'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Enable and edit e-mail messages sent to users when their accounts are canceled.') . ' ' . $email_token_help,
    '#group' => 'email',
  );
  $form['email_canceled']['user_mail_status_canceled_notify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify user when account is canceled.'),
    '#default_value' => $config->get('user_mail_status_canceled_notify'),
  );
  $form['email_canceled']['settings'] = array(
    '#type' => 'container',
    '#states' => array(
      // Hide the settings when the cancel notify checkbox is disabled.
      'invisible' => array(
        'input[name="user_mail_status_canceled_notify"]' => array('checked' => FALSE),
      ),
    ),
  );
  $form['email_canceled']['settings']['user_mail_status_canceled_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => $mail_config->get('status_canceled_subject'),
    '#maxlength' => 180,
  );
  $form['email_canceled']['settings']['user_mail_status_canceled_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => $mail_config->get('status_canceled_body'),
    '#rows' => 3,
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration')
  );
  return $form;
}

/**
 * Form submission handler for user_admin_settings().
 */
function user_admin_settings_submit($form, &$form_state) {
  config('system.core')
    ->set('anonymous', $form_state['values']['anonymous'])
    ->set('user_admin_role', $form_state['values']['user_admin_role'])
    ->set('user_cancel_method', $form_state['values']['user_cancel_method'])
    ->set('user_register', $form_state['values']['user_register'])
    ->set('user_email_verification', $form_state['values']['user_email_verification'])
    ->set('user_signatures', $form_state['values']['user_signatures'])
    ->set('user_mail_status_activated_notify', $form_state['values']['user_mail_status_activated_notify'])
    ->set('user_mail_status_blocked_notify', $form_state['values']['user_mail_status_blocked_notify'])
    ->set('user_mail_status_canceled_notify', $form_state['values']['user_mail_status_canceled_notify'])
    ->save();

  config('user.mail')
    ->set('cancel_confirm_body', $form_state['values']['user_mail_cancel_confirm_body'])
    ->set('cancel_confirm_subject', $form_state['values']['user_mail_cancel_confirm_subject'])
    ->set('password_reset_body', $form_state['values']['user_mail_password_reset_body'])
    ->set('password_reset_subject', $form_state['values']['user_mail_password_reset_subject'])
    ->set('register_admin_body', $form_state['values']['user_mail_register_admin_created_body'])
    ->set('register_admin_subject', $form_state['values']['user_mail_register_admin_created_subject'])
    ->set('register_no_approval_body', $form_state['values']['user_mail_register_no_approval_required_body'])
    ->set('register_no_approval_subject', $form_state['values']['user_mail_register_no_approval_required_subject'])
    ->set('register_pending_body', $form_state['values']['user_mail_register_pending_approval_body'])
    ->set('register_pending_subject', $form_state['values']['user_mail_register_pending_approval_subject'])
    ->set('status_activated_body', $form_state['values']['user_mail_status_activated_body'])
    ->set('status_activated_subject', $form_state['values']['user_mail_status_activated_subject'])
    ->set('status_blocked_body', $form_state['values']['user_mail_status_blocked_body'])
    ->set('status_blocked_subject', $form_state['values']['user_mail_status_blocked_subject'])
    ->set('status_canceled_body', $form_state['values']['user_mail_status_canceled_body'])
    ->set('status_canceled_subject', $form_state['values']['user_mail_status_canceled_subject'])
    ->save();

    backdrop_set_message(t('The configuration options have been saved.'));
}

/**
 * Menu callback: administer permissions.
 *
 * @ingroup forms
 * @see user_admin_permissions_submit()
 * @see theme_user_admin_permissions()
 */
function user_admin_permissions($form, $form_state, $role_name = NULL) {

  // Retrieve role names for columns.
  $roles = user_roles(FALSE, NULL, TRUE);
  if (isset($roles[$role_name])) {
    $roles = array($role_name => $roles[$role_name]);
  }
  $form['roles'] = array(
    '#type' => 'value',
    '#value' => $roles,
  );

  // Render role/permission overview:
  $options = array();
  $module_info = system_get_info('module');

  // Get a list of all the modules implementing a hook_permission() and sort by
  // display name.
  $modules = array();
  foreach (module_implements('permission') as $module) {
    $modules[$module] = $module_info[$module]['name'];
  }
  asort($modules);

  foreach ($modules as $module => $display_name) {
    if ($permissions = module_invoke($module, 'permission')) {
      $form['permission'][] = array(
        '#markup' => $module_info[$module]['name'],
        '#id' => $module,
      );
      foreach ($permissions as $perm => $perm_item) {
        // Fill in default values for the permission.
        $perm_item += array(
          'description' => '',
          'restrict access' => FALSE,
          'warning' => !empty($perm_item['restrict access']) ? t('Warning: Give to trusted roles only; this permission has security implications.') : '',
        );
        $options[$perm] = '';
        $form['permission'][$perm] = array(
          '#type' => 'item',
          '#markup' => $perm_item['title'],
          '#description' => theme('user_permission_description', array('permission_item' => $perm_item)),
        );
        foreach ($roles as $role_name => $role) {
          // Builds arrays for checked boxes for each role
          if (in_array($perm, $role->permissions)) {
            $status[$role_name][] = $perm;
          }
        }
      }
    }
  }

  // Have to build checkboxes here after checkbox arrays are built
  foreach ($roles as $role_name => $role) {
    $form['checkboxes'][$role_name] = array(
      '#type' => 'checkboxes',
      '#options' => $options,
      '#default_value' => isset($status[$role_name]) ? $status[$role_name] : array(),
      '#attributes' => array('class' => array('role-' . $role_name)),
    );
    $form['role_names'][$role_name] = array(
      '#markup' => check_plain($role->label),
      '#tree' => TRUE,
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save permissions'));

  $form['#attached']['js'][] = backdrop_get_path('module', 'user') . '/user.permissions.js';

  return $form;
}

/**
 * Save permissions selected on the administer permissions page.
 *
 * @see user_admin_permissions()
 */
function user_admin_permissions_submit($form, &$form_state) {
  foreach ($form_state['values']['roles'] as $role_name => $role) {
    user_role_change_permissions($role->name, $form_state['values'][$role->name]);
  }

  backdrop_set_message(t('The changes have been saved.'));
}

/**
 * Returns HTML for the administer permissions page.
 *
 * @param $variables
 *   An associative array containing:
 *   - form: A render element representing the form.
 *
 * @ingroup themeable
 */
function theme_user_admin_permissions($variables) {
  $form = $variables['form'];

  $roles = user_roles();
  foreach (element_children($form['permission']) as $key) {
    $row = array();
    // Module name
    if (is_numeric($key)) {
      $row[] = array('data' => backdrop_render($form['permission'][$key]), 'class' => array('module'), 'id' => 'module-' . $form['permission'][$key]['#id'], 'colspan' => count($form['roles']['#value']) + 1);
    }
    else {
      // Permission row.
      $row[] = array(
        'data' => backdrop_render($form['permission'][$key]),
        'class' => array('permission'),
      );
      foreach (element_children($form['checkboxes']) as $role_name) {
        $form['checkboxes'][$role_name][$key]['#title'] = $roles[$role_name] . ': ' . $form['permission'][$key]['#markup'];
        $form['checkboxes'][$role_name][$key]['#title_display'] = 'attribute';
        $row[] = array('data' => backdrop_render($form['checkboxes'][$role_name][$key]), 'class' => array('checkbox'));
      }
    }
    $rows[] = $row;
  }
  $header[] = (t('Permission'));
  foreach (element_children($form['role_names']) as $role_name) {
    $header[] = array('data' => backdrop_render($form['role_names'][$role_name]), 'class' => array('checkbox'));
  }
  $output = '';
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'permissions')));
  $output .= backdrop_render_children($form);
  return $output;
}

/**
 * Returns HTML for an individual permission description.
 *
 * @param $variables
 *   An associative array containing:
 *   - permission_item: An associative array representing the permission whose
 *     description is being themed. Useful keys include:
 *     - description: The text of the permission description.
 *     - warning: A security-related warning message about the permission (if
 *       there is one).
 *
 * @ingroup themeable
 */
function theme_user_permission_description($variables) {
  $description = array();
  $permission_item = $variables['permission_item'];
  if (!empty($permission_item['description'])) {
    $description[] = $permission_item['description'];
  }
  if (!empty($permission_item['warning'])) {
    $description[] = '<em class="permission-warning">' . $permission_item['warning'] . '</em>';
  }
  if (!empty($description)) {
    return implode(' ', $description);
  }
}

/**
 * Form to re-order roles or add a new one.
 *
 * @ingroup forms
 * @see theme_user_admin_roles()
 */
function user_admin_roles($form, $form_state) {
  $roles = user_roles(FALSE, NULL, TRUE);

  $form['roles'] = array(
    '#tree' => TRUE,
  );
  $order = 0;
  foreach ($roles as $role_name => $role) {
    $form['roles'][$role_name]['#role'] = $role;
    $form['roles'][$role_name]['#weight'] = $order;
    $form['roles'][$role_name]['weight'] = array(
      '#type' => 'textfield',
      '#title' => t('Weight for @title', array('@title' => check_plain($role->label))),
      '#title_display' => 'invisible',
      '#size' => 4,
      '#default_value' => $order,
      '#attributes' => array('class' => array('role-weight')),
    );
    $links = array();
    $links['edit'] = array(
      'title' => t('Edit role'),
      'href' => 'admin/config/people/roles/edit/' . $role_name,
      'weight' => 0,
    );
    $links['permissions'] = array(
      'title' => t('Edit permissions'),
      'href' => 'admin/config/people/permissions/' . $role_name,
      'weight' => 5,
    );
    if ($role_name !== BACKDROP_ANONYMOUS_ROLE && $role_name !== BACKDROP_AUTHENTICATED_ROLE) {
      $links['delete'] = array(
        'title' => t('Delete role'),
        'href' => 'admin/config/people/roles/delete/' . $role_name,
        'weight' => 0,
      );
    }
    $form['roles'][$role_name]['operations'] = array(
      '#type' => 'operations',
      '#links' => $links,
    );
    $order++;
  }

  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Add new role'),
    '#title_display' => 'invisible',
    '#required' => TRUE,
    '#size' => 32,
    '#maxlength' => 64,
  );
  $form['name'] = array(
    '#type' => 'machine_name',
    '#required' => TRUE,
    '#size' => 32,
    '#maxlength' => 64,
    '#machine_name' => array(
      'source' => array('label'),
      'exists' => 'user_role_load',
      'standalone' => TRUE,
      'label' => '',
    ),
    '#description' => t('A unique machine-readable name for this role. It must only contain lowercase letters, numbers, and underscores.'),
  );
  $form['add'] = array(
    '#type' => 'submit',
    '#value' => t('Add role'),
    '#submit' => array('user_admin_roles_add_submit'),
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save order'),
    '#limit_validation_errors' => array(array('roles')),
    '#submit' => array('user_admin_roles_order_submit'),
  );

  return $form;
}

/**
 * Form submit function. Update the role weights.
 */
function user_admin_roles_order_submit($form, &$form_state) {
  foreach ($form_state['values']['roles'] as $role_name => $role_values) {
    $role = $form['roles'][$role_name]['#role'];
    $role->weight = $role_values['weight'];
    user_role_save($role);
  }
  backdrop_set_message(t('The role settings have been updated.'));
}

/**
 * Form submit function. Create a new user role.
 */
function user_admin_roles_add_submit($form, &$form_state) {
  $role = (object) array(
    'name' => $form_state['values']['name'],
    'label' => $form_state['values']['label'],
  );
  user_role_save($role);
  backdrop_set_message(t('The role has been added.'));
  $form_state['redirect'] = 'admin/config/people/roles';
}

/**
 * Returns HTML for the role order and new role form.
 *
 * @param $variables
 *   An associative array containing:
 *   - form: A render element representing the form.
 *
 * @ingroup themeable
 */
function theme_user_admin_roles($variables) {
  $form = $variables['form'];

  $header = array(t('Name'), t('Weight'), t('Operations'));
  foreach (element_children($form['roles']) as $role_name) {
    $label = $form['roles'][$role_name]['#role']->label;
    $row = array();
    if (in_array($role_name, array(BACKDROP_ANONYMOUS_ROLE, BACKDROP_AUTHENTICATED_ROLE))) {
      $row[] = t('@name <em>(required)</em>', array('@name' => $label));
    }
    else {
      $row[] = check_plain($label);
    }
    $row[] = backdrop_render($form['roles'][$role_name]['weight']);
    $row[] = backdrop_render($form['roles'][$role_name]['operations']);
    $rows[] = array('data' => $row, 'class' => array('draggable'));
  }
  $row = array();
  $row[] = array('data' => backdrop_render($form['label']) . backdrop_render($form['name']), 'class' => 'edit-name');
  $row[] = '';
  $row[] = backdrop_render($form['add']);
  $rows[] = $row;

  backdrop_add_tabledrag('user-roles', 'order', 'sibling', 'role-weight');

  $output = theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'user-roles')));
  $output .= backdrop_render_children($form);

  return $output;
}

/**
 * Form to configure a single role.
 *
 * @ingroup forms
 * @see user_admin_role_submit()
 */
function user_admin_role($form, $form_state, $role) {
  $form_state['role'] = $role;

  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Role name'),
    '#default_value' => $role->label,
    '#size' => 30,
    '#required' => TRUE,
    '#maxlength' => 64,
    '#description' => t('The name for this role. Example: "moderator", "editorial board", "site architect".'),
  );
  $form['name'] = array(
    '#type' => 'machine_name',
    '#size' => 32,
    '#maxlength' => 64,
    '#default_value' => $role->name,
    '#disabled' => TRUE,
    '#machine_name' => array(
      'exists' => 'user_role_load',
    ),
    '#description' => t('A unique machine-readable name for this role. It must only contain lowercase letters, numbers, and underscores.'),
  );
  $form['weight'] = array(
    '#type' => 'value',
    '#value' => $role->weight,
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save role'),
  );

  return $form;
}

/**
 * Form submit handler for the user_admin_role() form.
 */
function user_admin_role_submit($form, &$form_state) {
  $role = $form_state['role'];
  $role->label = $form_state['values']['label'];
  user_role_save($role);
  backdrop_set_message(t('The role has been renamed.'));
  $form_state['redirect'] = 'admin/config/people/roles';
}

/**
 * Form to confirm role delete operation.
 */
function user_admin_role_delete_confirm($form, &$form_state, $role) {
  $form['role_name'] = array(
    '#type' => 'value',
    '#value' => $role->name,
  );
  return confirm_form($form, t('Are you sure you want to delete the role %name ?', array('%name' => $role->label)), 'admin/people/roles', t('This action cannot be undone.'), t('Delete'));
}

/**
 * Form submit handler for user_admin_role_delete_confirm().
 */
function user_admin_role_delete_confirm_submit($form, &$form_state) {
  user_role_delete($form_state['values']['role_name']);
  backdrop_set_message(t('The role has been deleted.'));
  $form_state['redirect'] = 'admin/config/people/roles';
}

