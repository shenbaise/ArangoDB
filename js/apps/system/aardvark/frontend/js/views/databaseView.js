/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.databaseView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("databaseView.ejs"),

    currentDB: "",

    events: {
      "click #createDatabase"                           : "createDatabase",
      "click #submitCreateDatabase"                     : "submitCreateDatabase",
      "click #selectDatabase"                           : "updateDatabase",
      "click #databaseTable .icon_arangodb_roundminus"  : "removeDatabase",
      "click #submitDeleteDatabase"                     : "submitRemoveDatabase",
      "click .contentRowInactive a"                     : "changeDatabase"
    },

    initialize: function() {
      this.collection.fetch({async:false});
    },

    render: function(){
      this.currentDatabase();
      $(this.el).html(this.template.render({}));
      this.renderTable();
      this.selectCurrentDatabase();
      return this;
    },

    renderTable: function () {
      this.collection.forEach(function(dbs) {
        $("#databaseTable tbody").append(
          '<tr><td><a id="' + dbs.get("name") + '">' + dbs.get("name") + '</a></td>' +
          '<td><span class="arangoicon icon_arangodb_roundminus"' + 
          'data-original-title="Delete database"></span></td></tr>'
        );
      });
    },

    selectedDatabase: function () {
      return $('#selectDatabases').val();
    },

    handleError: function(status, text, dbname) {
      if (status === 409) {
        arangoHelper.arangoError("Database " + dbname + " already exists.");
        return;
      }
      if (status === 400) {
        arangoHelper.arangoError("Invalid Parameters");
        return;
      }
      if (status === 403) {
        arangoHelper.arangoError("Insufficent rights. Execute this from _system database");
        return;
      }
    },

    validateDatabaseInfo: function (db, user, pw) {
      if (user === "") {
        arangoHelper.arangoError("You have to define an owner for the new database");
        return false;
      }
      if (db === "") {
        arangoHelper.arangoError("You have to define a name for the new database"); 
        return false;
      }
      if (db.indexOf("_") === 0) {
        arangoHelper.arangoError("Databasename should not start with _");
        return false;
      }
      if (!db.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Databasename may only contain numbers, letters, _ and -");
        return false;
      }
      return true;
    },

    submitCreateDatabase: function() {
      var self = this;
      var name  = $('#newDatabaseName').val();
      var userName = $('#newUser').val();
      var userPassword = $('#newPassword').val();
      if (!this.validateDatabaseInfo(name, userName, userPassword)) {
        return;
      }
      var options = {
        name: name,
        users: [
          {
            username: userName,
            passwd: userPassword,
            active: true
          }
        ]
      };
      this.collection.create(options, {
        wait:true,
        error: function(data, err) {
          self.handleError(err.status, err.statusText, name);
        },
        success: function(data) {
          self.hideModal();
          self.updateDatabases();
        }
      });
    },

    hideModal: function() {
      $('#createDatabaseModal').modal('hide');
    },

    showModal: function() {
      $('#createDatabaseModal').modal('show');
    },

    createDatabase: function() {
      this.showModal();
    },

    submitRemoveDatabase: function(e) {
      var toDelete = this.collection.where({name: this.dbToDelete});
      toDelete[0].destroy({wait: true, url:"/_api/database/"+this.dbToDelete});
      this.dbToDelete = '';
      $('#deleteDatabaseModal').modal('hide');
      this.updateDatabases();
    },

    removeDatabase: function(e) {
      this.dbToDelete = $(e.currentTarget).parent().parent().children().first().text();
      $('#deleteDatabaseModal').modal('show');
    },

    currentDatabase: function() {
      this.currentDB = this.collection.getCurrentDatabase();
    },

    selectCurrentDatabase: function() {
      $('#databaseTableBody tr').addClass('contentRowInactive');
      var tr = $('#databaseTableBody td:contains('+this.currentDB+')').parent();
      $(tr).removeClass('contentRowInactive').addClass('contentRowActive');
    },

    changeDatabase: function(e) {
      var changeTo = $(e.currentTarget).attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    updateDatabases: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
          window.App.handleSelectDatabase();
        }
      });
    }

  });
}());
