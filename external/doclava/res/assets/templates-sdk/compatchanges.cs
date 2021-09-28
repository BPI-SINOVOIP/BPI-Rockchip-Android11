<?cs # THIS CREATES THE COMPAT CONFIG DOCS FROM compatconfig.xml ?>
<?cs include:"macros.cs" ?>
<?cs include:"macros_override.cs" ?>

<?cs include:"doctype.cs" ?>
<html<?cs if:devsite ?> devsite<?cs /if ?>>
<?cs include:"head_tag.cs" ?>
<?cs include:"body_tag.cs" ?>
<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<?cs include:"header.cs" ?>
<?cs # Includes api-info-block DIV at top of page. Standard Devsite uses right nav. ?>
<?cs if:dac ?><?cs include:"page_info.cs" ?><?cs /if ?>
<?cs # This DIV spans the entire document to provide scope for some scripts ?>
<div id="jd-content" >

<?cs each:change=change ?>
  <h3 class="api-name" id="<?cs var:change.name ?>"><?cs var:change.name ?></h3>
  <div>Value: <?cs var:change.id ?></div>
  <div>
  <?cs if:change.loggingOnly ?>
        Used for logging only.
  <?cs else ?>
        <?cs if:change.disabled ?>
            Disabled for all apps.
        <?cs else ?>
            Enabled for
            <?cs if:change.enableAfterTargetSdk ?>
                apps with a <code>targetSdkVersion</code> of greater than
                <?cs var:change.enableAfterTargetSdk ?>.
            <?cs else ?>
                all apps.
            <?cs /if ?>
        <?cs /if ?>
  <?cs /if ?>
  </div>

  <?cs call:description(change) ?>
<?cs /each ?>

</div>
<?cs if:!devsite ?>
<?cs include:"footer.cs" ?>
<?cs include:"trailer.cs" ?>
<?cs /if ?>
</div><!-- end devsite ReferenceObject -->
</body>
</html>
